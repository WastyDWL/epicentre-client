#include "BrowserApi.h"

#include <windows.h>
#include <shellapi.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <FileSystem.h>
#include <GameServerHelpers.h>
#include <GameUi.h>
#include <IGameUIFuncs.h>
#include <tier2/tier2.h>
#include <vgui/ILocalize.h>
#include <TaskRun.h>
#include <chromehtml/ncl_browser_api.h>
#include <nitro_utils/net_utils.h>
#include <steam/steam_api.h>

#include "BasePanel.h"
#include "MainMenuBrowser.h"
#include "ModInfo.h"

// GameUi.cpp: the game window
HWND GameUI_MainWindow();

namespace {

using ncl::IBrowserApiReply;

ncl::IBrowserApi* g_api = nullptr;
std::string g_options_tab;
std::string g_menu_command;  // a gameUi.command() to run from GameUI's frame

std::string json_string(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '<': out += "\\u003c"; break;
            case '>': out += "\\u003e"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

const char* json_bool(bool v) { return v ? "true" : "false"; }

// Same shape as NextClient's GameServerItemToV8Object.
std::string server_json(const gameserveritem_t& s) {
    return "{\"appId\":" + std::to_string(s.m_nAppID) + ",\"gameDir\":" + json_string(s.m_szGameDir) +
           ",\"address\":" + json_string(s.m_NetAdr.GetConnectionAddressString()) +
           ",\"hostname\":" + json_string(s.GetName()) + ",\"map\":" + json_string(s.m_szMap) +
           ",\"playersOnline\":" + std::to_string(GetHumanPlayerCount(s)) +
           ",\"botsOnline\":" + std::to_string(s.m_nBotPlayers) + ",\"playersMax\":" + std::to_string(s.m_nMaxPlayers) +
           ",\"isPasswordProtected\":" + json_bool(s.m_bPassword) + ",\"isVacSecured\":" + json_bool(s.m_bSecure) +
           ",\"unixTimeLastPlayed\":" + std::to_string(s.m_ulTimeLastPlayed) +
           ",\"ping\":" + std::to_string(s.m_nPing) + "}";
}

// Steam callbacks may arrive outside the frame; replies always go back on the main thread.
void resolve(IBrowserApiReply* reply, std::string json) {
    TaskCoro::RunInMainThread([reply, json = std::move(json)] { reply->Resolve(json.c_str()); });
}
void reject(IBrowserApiReply* reply, int status) {
    TaskCoro::RunInMainThread([reply, status] { reply->Reject(status); });
}

// ---- server rules ----

class RulesQuery final : public ISteamMatchmakingRulesResponse {
public:
    RulesQuery(IBrowserApiReply* reply, uint32 ip, uint16 port) : reply_(reply) {
        handle_ = SteamMatchmakingServers()->ServerRules(ip, port, this);
    }
    void RulesResponded(const char* rule, const char* value) override {
        if (!rules_.empty()) rules_ += ",";
        rules_ += "{\"rule\":" + json_string(rule) + ",\"value\":" + json_string(value) + "}";
    }
    void RulesFailedToRespond() override { finish(false); }
    void RulesRefreshComplete() override { finish(true); }

private:
    void finish(bool ok) {
        if (ok) resolve(reply_, "[" + rules_ + "]");
        else reject(reply_, 504);
        TaskCoro::RunInMainThread([this] { delete this; });
    }
    IBrowserApiReply* reply_;
    HServerQuery handle_ = HSERVERQUERY_INVALID;
    std::string rules_;
};

// ---- favorites / history / internet lists ----

enum class ListKind { Favorites, History, Internet };

class ListQuery;
std::set<ListQuery*> g_lists;  // running list queries, for the deadline check in BrowserApi_RunFrame

const char* list_name(ListKind kind) {
    switch (kind) {
        case ListKind::Favorites: return "favorites";
        case ListKind::History: return "history";
        default: return "internet";
    }
}

// Servers that answered since the last frame, per list: streamed to pages as
// nextclient.matchmaking 'servers' events so lists fill in while the query runs.
std::map<std::string, std::vector<std::string>> g_batches;

// Answers with the servers that responded so far once the deadline passes: pinging a whole master
// list takes much longer than a menu should wait.
constexpr auto kListDeadline = std::chrono::milliseconds(3500);

class ListQuery final : public ISteamMatchmakingServerListResponse {
public:
    ListQuery(IBrowserApiReply* reply, ListKind kind)
        : reply_(reply), kind_(kind), deadline_(std::chrono::steady_clock::now() + kListDeadline) {
        g_lists.insert(this);
        auto app = SteamUtils()->GetAppID();
        auto* mm = SteamMatchmakingServers();
        switch (kind) {
            case ListKind::Favorites: request_ = mm->RequestFavoritesServerList(app, nullptr, 0, this); break;
            case ListKind::History: request_ = mm->RequestHistoryServerList(app, nullptr, 0, this); break;
            case ListKind::Internet: request_ = mm->RequestInternetServerList(app, nullptr, 0, this); break;
        }
    }
    ~ListQuery() {
        g_lists.erase(this);
        if (request_) {
            SteamMatchmakingServers()->CancelQuery(request_);
            SteamMatchmakingServers()->ReleaseRequest(request_);
        }
    }
    void ServerResponded(HServerListRequest request, int index) override { add(request, index, true); }
    void ServerFailedToRespond(HServerListRequest request, int index) override { add(request, index, false); }
    void RefreshComplete(HServerListRequest, EMatchMakingServerResponse response) override {
        finish(response != eServerFailedToRespond || !servers_.empty());
    }
    bool expired() const { return std::chrono::steady_clock::now() >= deadline_; }
    // Answers once (complete, or partial at the deadline) and schedules deletion.
    void finish(bool ok) {
        if (done_) return;
        done_ = true;
        if (!ok) {
            reject(reply_, 504);
        } else {
            std::string out = "[";
            bool first = true;
            for (auto& [index, item] : servers_) {
                if (!first) out += ",";
                first = false;
                out += item;
            }
            resolve(reply_, out + "]");
        }
        TaskCoro::RunInMainThread([this] { delete this; });
    }

private:
    void add(HServerListRequest request, int index, bool responded) {
        // The internet list only shows servers that answered; favorites/history keep offline ones.
        if (!responded && kind_ == ListKind::Internet) return;
        gameserveritem_t* s = SteamMatchmakingServers()->GetServerDetails(request, index);
        if (!s) return;
        std::string item = "{\"address\":" + json_string(s->m_NetAdr.GetConnectionAddressString()) +
                           ",\"info\":" + (responded ? server_json(*s) : std::string("null")) + "}";
        if (responded && !done_) g_batches[list_name(kind_)].push_back(item);
        servers_[index] = std::move(item);
    }
    IBrowserApiReply* reply_;
    const ListKind kind_;
    const std::chrono::steady_clock::time_point deadline_;
    HServerListRequest request_ = nullptr;
    std::map<int, std::string> servers_;
    bool done_ = false;
};

// ---- handlers (main thread) ----

bool trusted(const char* frame_url) { return g_api && g_api->IsTrusted(frame_url); }

void on_rules(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    uint32_t ip;
    uint16_t port;
    if (!nitro_utils::ParseAddress(arg, ip, port, true)) return reply->Reject(400);
    new RulesQuery(reply, ip, port);
}

void on_list(void* kind, const char*, const char*, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    new ListQuery(reply, static_cast<ListKind>(reinterpret_cast<intptr_t>(kind)));
}

void on_favorite(void* add, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    uint32_t ip;
    uint16_t port;
    if (!nitro_utils::ParseAddress(arg, ip, port, true)) return reply->Reject(400);
    auto app = SteamUtils()->GetAppID();
    if (add) SteamMatchmaking()->AddFavoriteGame(app, ip, port, port, k_unFavoriteFlagFavorite, 0);
    else SteamMatchmaking()->RemoveFavoriteGame(app, ip, port, port, k_unFavoriteFlagFavorite);
    reply->Resolve("true");
}

// The game menu's commands, for the Epicentre menu page (the vgui game menu is hidden).
bool known_menu_command(const std::string& c) {
    return c == "NewGame" || c == "Options" || c == "Players" || c == "Resume" || c == "Disconnect" ||
           c == "Quit" || c == "QuitNoConfirm" || c == "ServerBrowser";
}

void on_menu_command(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    std::string c = arg ? arg : "";
    if (!known_menu_command(c)) return reply->Reject(400);
    g_menu_command = c;
    reply->Resolve("true");
}

void run_menu_command(const std::string& c) {
    CBasePanel* panel = BasePanel();
    if (c == "NewGame") panel->OnOpenCreateMultiplayerGameDialog();
    else if (c == "Options") panel->OnOpenOptionsDialog();
    else if (c == "Players") panel->OnOpenPlayerListDialog();
    else if (c == "ServerBrowser") panel->OnOpenServerBrowser();
    else if (c == "Quit") panel->OnOpenQuitConfirmationDialog();
    else if (c == "QuitNoConfirm") engine->pfnClientCmd("quit\n");
    else if (c == "Disconnect") engine->pfnClientCmd("disconnect\n");
    else if (c == "Resume") engine->pfnClientCmd("cancelselect\n");  // as the game menu's ResumeGame
}

// Links the menu hands to the user's own browser (VK asks for a login inside the game view).
bool external_url_allowed(const std::string& url) {
    static const char* const kPrefixes[] = {"https://vk.ru/", "https://vk.com/", "https://epicentre-cs.ru/"};
    if (url.size() > 512) return false;
    for (unsigned char c : url)
        if (c <= ' ' || c == '"' || c == '\\' || c >= 0x7f) return false;
    for (const char* p : kPrefixes)
        if (url.rfind(p, 0) == 0) return true;
    return false;
}

void on_open_external(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    std::string url = arg ? arg : "";
    if (!external_url_allowed(url)) return reply->Reject(400);
    std::wstring wide(url.begin(), url.end());  // ASCII only, checked above
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    reply->Resolve("true");
}

// ---- site account: the client API token (api/client on epicentre-cs.ru) ----
// Kept in the game folder, encrypted for the Windows user (DPAPI). Only the built-in menu reads
// or writes it: site pages are trusted for other calls but never see the token.

constexpr const char* kAccountFile = "epicentre_account.dat";

bool menu_frame(const char* frame_url) {
    return frame_url && std::string_view(frame_url).rfind("ncl://menu/", 0) == 0;
}

bool token_valid(const std::string& token) {
    if (token.size() != 64) return false;
    for (char c : token)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

std::string read_account_token() {
    FILE* f = fopen(kAccountFile, "rb");
    if (!f) return {};
    std::vector<BYTE> data;
    BYTE buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && data.size() < 8192) data.insert(data.end(), buf, buf + n);
    fclose(f);

    DATA_BLOB in{static_cast<DWORD>(data.size()), data.data()}, out{};
    if (data.empty() || !CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    std::string token(reinterpret_cast<char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return token_valid(token) ? token : std::string();
}

bool write_account_token(const std::string& token) {
    if (token.empty()) return remove(kAccountFile) == 0 || errno == ENOENT;
    DATA_BLOB in{static_cast<DWORD>(token.size()), reinterpret_cast<BYTE*>(const_cast<char*>(token.data()))}, out{};
    if (!CryptProtectData(&in, L"Epicentre account", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return false;
    FILE* f = fopen(kAccountFile, "wb");
    bool ok = f && fwrite(out.pbData, 1, out.cbData, f) == out.cbData;
    if (f) fclose(f);
    LocalFree(out.pbData);
    return ok;
}

void on_account_token(void*, const char*, const char*, const char* frame_url, IBrowserApiReply* reply) {
    if (!menu_frame(frame_url)) return reply->Reject(403);
    std::string token = read_account_token();
    reply->Resolve(token.empty() ? "null" : json_string(token).c_str());
}

// The menu page (ncl://) cannot fetch the site itself: Chromium refuses cross-origin requests from
// the custom scheme. The game makes the request instead (WinHTTP, system proxy) and adds the saved
// token, so the page never handles it. arg: the form body ("action=me&..."); the answer is
// {"status": HTTP status (0 when the site was not reached), "body": text, "error": text}.
constexpr const wchar_t* kSiteHost = L"epicentre-cs.ru";
constexpr const wchar_t* kSiteApiPath = L"/api/client/";

// One session and connection for the whole run: WinHTTP keeps the TLS connection alive between
// calls, so only the first request pays for the handshake. Handles are thread-safe and live until exit.
HINTERNET site_connection() {
    static HINTERNET connect = [] {
        HINTERNET session = WinHttpOpen(L"EpicentreClient/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)  // before Windows 8.1 there is no automatic proxy
            session = WinHttpOpen(L"EpicentreClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
        return session ? WinHttpConnect(session, kSiteHost, INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    }();
    return connect;
}

std::string site_post(const std::string& body, int& status, std::string& error) {
    status = 0;
    std::string out;
    HINTERNET connect = site_connection();
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"POST", kSiteApiPath, nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : nullptr;
    if (request) {
        WinHttpSetTimeouts(request, 10000, 10000, 15000, 20000);
        const wchar_t* headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
        if (WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L), const_cast<char*>(body.data()),
                               static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
            WinHttpReceiveResponse(request, nullptr)) {
            DWORD code = 0, size = sizeof(code);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                &code, &size, WINHTTP_NO_HEADER_INDEX);
            status = static_cast<int>(code);
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(request, &avail) && avail > 0 && out.size() < (4u << 20)) {
                std::string chunk(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(request, chunk.data(), avail, &read) || read == 0) break;
                out.append(chunk.data(), read);
            }
        }
    }
    if (status == 0) error = "WinHTTP error " + std::to_string(GetLastError());
    if (request) WinHttpCloseHandle(request);
    return out;
}

void on_site_api(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!menu_frame(frame_url)) return reply->Reject(403);
    std::string body = arg ? arg : "";
    if (body.size() > 16384) return reply->Reject(400);
    // the token goes with every call but sign-in (read here, on the main thread)
    if (body.rfind("action=login&", 0) != 0) {
        std::string token = read_account_token();
        if (!token.empty()) body += "&token=" + token;
    }
    std::thread([reply, body = std::move(body)] {
        int status = 0;
        std::string error;
        std::string text = site_post(body, status, error);
        resolve(reply, "{\"status\":" + std::to_string(status) + ",\"body\":" + json_string(text) +
                           ",\"error\":" + json_string(error) + "}");
    }).detach();
}

// ---- the battle pass mini game, embedded in the menu ----
// A native Godot build in epicentre_games/xmas (the web build in the menu's browser runs at a
// fraction of the frame rate). The game starts off screen; once its window shows up it becomes a
// child of the game window, kept over the rectangle the page keeps free (browser pixels), the way
// the Godot editor embeds a running game. It closes with the menu or when the page asks.
constexpr const wchar_t* kXmasGameExe = L"epicentre_games\\xmas\\XmasGame.exe";

struct EmbeddedGame {
    HANDLE process = nullptr;
    DWORD pid = 0;
    HWND wnd = nullptr;
    bool fg_locked = false;  // LockSetForegroundWindow until the window is embedded
    int x = 0, y = 0, w = 0, h = 0;  // browser pixels
    RECT placed{};
    std::chrono::steady_clock::time_point started, kill_at;
    bool closing = false;
} g_game;

HWND find_process_window(DWORD pid) {
    struct Search {
        DWORD pid;
        HWND found;
    } search{pid, nullptr};
    EnumWindows(
        [](HWND wnd, LPARAM param) -> BOOL {
            auto* s = reinterpret_cast<Search*>(param);
            DWORD owner = 0;
            GetWindowThreadProcessId(wnd, &owner);
            if (owner != s->pid || !IsWindowVisible(wnd)) return TRUE;
            s->found = wnd;
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.found;
}

// the page's rectangle in the game window's client coordinates
RECT game_target_rect() {
    int x = g_game.x, y = g_game.y;
    CMainMenuBrowser* browser = BasePanel() ? BasePanel()->GetMainMenuBrowser() : nullptr;
    if (browser) browser->LocalToScreen(x, y);
    return RECT{x, y, x + g_game.w, y + g_game.h};
}

void place_game() {
    if (!g_game.wnd) return;
    RECT r = game_target_rect();
    if (EqualRect(&r, &g_game.placed)) return;
    g_game.placed = r;
    MoveWindow(g_game.wnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}

// Input: the CS window keeps the keyboard focus (SDL minimizes a full-screen game whose window
// loses it), so the embedded window is disabled and gets nothing from the system. The page takes
// the keys and the mouse over its stage and forwards them (gameInput), posted to the game window.
void unlock_foreground() {
    if (!g_game.fg_locked) return;
    LockSetForegroundWindow(LSFW_UNLOCK);
    g_game.fg_locked = false;
}

// back to a clean state after the process ended
void reset_game() {
    unlock_foreground();
    if (g_game.process) CloseHandle(g_game.process);
    g_game = EmbeddedGame{};
}

void embed_game(HWND wnd) {
    g_game.wnd = wnd;
    LONG_PTR style = GetWindowLongPtrW(wnd, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    SetWindowLongPtrW(wnd, GWL_STYLE, style | WS_CHILD | WS_CLIPSIBLINGS | WS_DISABLED);
    SetParent(wnd, GameUI_MainWindow());
    g_game.placed = RECT{};
    place_game();
    ShowWindow(wnd, SW_SHOWNOACTIVATE);
    unlock_foreground();
}

void close_game() {
    if (!g_game.process || g_game.closing) return;
    g_game.closing = true;
    g_game.kill_at = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    if (g_game.wnd) PostMessageW(g_game.wnd, WM_CLOSE, 0, 0);
}

// RunFrame: embed the window once it exists, follow the rectangle, clean up after exit
void run_game_frame() {
    if (!g_game.process) return;
    auto now = std::chrono::steady_clock::now();
    if (WaitForSingleObject(g_game.process, 0) == WAIT_OBJECT_0 ||
        (g_game.closing && now >= g_game.kill_at && TerminateProcess(g_game.process, 0))) {
        reset_game();
        return;
    }
    if (g_game.closing) return;
    // the menu went away (back to the match): the game goes with it
    if (!BasePanel() || !BasePanel()->IsVisible()) return close_game();
    if (!g_game.wnd) {
        if (HWND wnd = find_process_window(g_game.pid)) embed_game(wnd);
        else if (now - g_game.started > std::chrono::seconds(30)) close_game();
        if (now - g_game.started > std::chrono::seconds(10)) unlock_foreground();
        return;
    }
    place_game();
}

bool parse_rect(const char* text, int& x, int& y, int& w, int& h) {
    return text && sscanf(text, "%d %d %d %d", &x, &y, &w, &h) == 4 && w > 0 && h > 0 && w < 16384 && h < 16384;
}

// arg: "<game token>\t<login, URI-encoded>\t<x y w h>"; the token comes from the site (api/client
// game_token) and only opens runs, the top and ticket purchases. Rejects 404 when the build is not
// installed, so the menu falls back to the site's page.
void on_launch_game(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!menu_frame(frame_url)) return reply->Reject(403);
    std::string a = arg ? arg : "";
    size_t t1 = a.find('\t'), t2 = t1 == std::string::npos ? t1 : a.find('\t', t1 + 1);
    if (t2 == std::string::npos) return reply->Reject(400);
    std::string token = a.substr(0, t1), login = a.substr(t1 + 1, t2 - t1 - 1);
    int x, y, w, h;
    if (!token_valid(token) || login.size() > 192 || !parse_rect(a.c_str() + t2 + 1, x, y, w, h))
        return reply->Reject(400);
    for (unsigned char c : login)  // URI-encoded: nothing to quote on the command line
        if (!isalnum(c) && !strchr("%-._~", c)) return reply->Reject(400);

    if (g_game.process && !g_game.closing) {  // one game at a time: "Play" again brings it back
        g_game.x = x, g_game.y = y, g_game.w = w, g_game.h = h;
        place_game();
        return reply->Resolve("true");
    }

    wchar_t dir[MAX_PATH];
    DWORD n = GetCurrentDirectoryW(MAX_PATH, dir);
    if (n == 0 || n >= MAX_PATH) return reply->Reject(500);
    std::wstring exe = std::wstring(dir) + L"\\" + kXmasGameExe;
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) return reply->Reject(404);
    std::wstring work = exe.substr(0, exe.find_last_of(L'\\'));

    // off screen until it is embedded, already at its size
    std::string args = " --position -32000,-32000 --resolution " + std::to_string(w) + "x" + std::to_string(h) +
                       " -- --epic-token=" + token + " --epic-login=" + login;
    std::wstring cmd = L"\"" + exe + L"\"" + std::wstring(args.begin(), args.end());  // ASCII, checked above
    if (g_game.process) {  // the previous one is still closing
        TerminateProcess(g_game.process, 0);
        reset_game();
    }
    // The game's window must not take the foreground: CS in full screen minimizes when it loses it.
    // The lock holds until the window is embedded (a child never becomes the foreground window).
    bool fg_locked = LockSetForegroundWindow(LSFW_LOCK) != FALSE;
    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, work.c_str(), &si, &pi)) {
        if (fg_locked) LockSetForegroundWindow(LSFW_UNLOCK);
        return reply->Reject(500);
    }
    CloseHandle(pi.hThread);
    g_game = EmbeddedGame{};
    g_game.fg_locked = fg_locked;
    g_game.process = pi.hProcess;
    g_game.pid = pi.dwProcessId;
    g_game.x = x, g_game.y = y, g_game.w = w, g_game.h = h;
    g_game.started = std::chrono::steady_clock::now();
    reply->Resolve("true");
}

// arg: "x y w h" in browser pixels (the page resized)
void on_set_game_rect(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!menu_frame(frame_url)) return reply->Reject(403);
    int x, y, w, h;
    if (!parse_rect(arg, x, y, w, h)) return reply->Reject(400);
    g_game.x = x, g_game.y = y, g_game.w = w, g_game.h = h;
    place_game();
    reply->Resolve("true");
}

// arg: "k <down 0|1> <virtual key> <repeat 0|1>" or "m <WM_ mouse message> <x> <y> <MK_ flags>"
// (x, y in device pixels of the game's area)
void on_game_input(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!menu_frame(frame_url)) return reply->Reject(403);
    if (!g_game.wnd || g_game.closing) return reply->Resolve("false");
    char kind = 0;
    int a = 0, b = 0, c = 0, d = 0;
    int n = arg ? sscanf(arg, "%c %d %d %d %d", &kind, &a, &b, &c, &d) : 0;
    if (kind == 'k' && n >= 4 && b > 0 && b < 256) {
        UINT scan = MapVirtualKeyW(static_cast<UINT>(b), MAPVK_VK_TO_VSC);
        bool extended = b == VK_LEFT || b == VK_RIGHT || b == VK_UP || b == VK_DOWN || b == VK_INSERT ||
                        b == VK_DELETE || b == VK_HOME || b == VK_END || b == VK_PRIOR || b == VK_NEXT;
        LPARAM lp = 1 | (static_cast<LPARAM>(scan) << 16) | (extended ? (1 << 24) : 0);
        if (!a) lp |= 0xC0000000;          // key up: previous state down, transition
        else if (c) lp |= 0x40000000;      // auto-repeat: previously down
        PostMessageW(g_game.wnd, a ? WM_KEYDOWN : WM_KEYUP, static_cast<WPARAM>(b), lp);
    } else if (kind == 'm' && n == 5 && (a == WM_MOUSEMOVE || (a >= WM_LBUTTONDOWN && a <= WM_MBUTTONUP))) {
        PostMessageW(g_game.wnd, static_cast<UINT>(a), static_cast<WPARAM>(d), MAKELPARAM(b, c));
    } else {
        return reply->Reject(400);
    }
    reply->Resolve("true");
}

void on_close_game(void*, const char*, const char*, const char* frame_url, IBrowserApiReply* reply) {
    if (!menu_frame(frame_url)) return reply->Reject(403);
    close_game();
    reply->Resolve("true");
}

void on_game_running(void*, const char*, const char*, const char* frame_url, IBrowserApiReply* reply) {
    if (!menu_frame(frame_url)) return reply->Reject(403);
    reply->Resolve(g_game.process && !g_game.closing ? "true" : "false");
}

// arg: the new token, or empty to sign out
void on_set_account_token(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!menu_frame(frame_url)) return reply->Reject(403);
    std::string token = arg ? arg : "";
    if (!token.empty() && !token_valid(token)) return reply->Reject(400);
    if (!write_account_token(token)) return reply->Reject(500);
    reply->Resolve("true");
}

// ---- settings page: video modes and key bindings (the menu's own options screen) ----

std::string utf8_of(const wchar_t* w) {
    if (!w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return out;
}

// "#Token" through the game's localization, anything else as it is
std::string localized(const std::string& text) {
    if (text.empty() || text[0] != '#' || !g_pVGuiLocalize) return text;
    const wchar_t* w = g_pVGuiLocalize->Find(text.c_str());
    return w ? utf8_of(w) : text;
}

// Quoted or bare tokens of a gfx/shell list file, "//" comments skipped.
std::vector<std::string> list_tokens(const char* path) {
    std::vector<std::string> out;
    FileHandle_t fh = g_pFullFileSystem->Open(path, "rb");
    if (fh == FILESYSTEM_INVALID_HANDLE) return out;
    std::string data(g_pFullFileSystem->Size(fh), '\0');
    g_pFullFileSystem->Read(data.data(), static_cast<int>(data.size()), fh);
    g_pFullFileSystem->Close(fh);

    size_t i = 0;
    while (i < data.size()) {
        unsigned char c = data[i];
        if (c <= ' ') { i++; continue; }
        if (c == '/' && i + 1 < data.size() && data[i + 1] == '/') {
            while (i < data.size() && data[i] != '\n') i++;
            continue;
        }
        std::string tok;
        if (c == '"') {
            for (i++; i < data.size() && data[i] != '"'; i++) tok += data[i];
            i++;
        } else {
            for (; i < data.size() && static_cast<unsigned char>(data[i]) > ' '; i++) tok += data[i];
        }
        out.push_back(tok);
    }
    return out;
}

struct VideoState { int w = 0, h = 0, bpp = 32, windowed = 0, low = 0; };

VideoState current_video() {
    VideoState v;
    char renderer[128];
    int hd = 0, addons = 0;
    g_pGameUIFuncs->GetCurrentVideoMode(&v.w, &v.h, &v.bpp);
    g_pGameUIFuncs->GetCurrentRenderer(renderer, sizeof(renderer), &v.windowed, &hd, &addons, &v.low);
    return v;
}

void on_video_state(void*, const char*, const char*, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    vmode_t* modes = nullptr;
    int count = 0;
    g_pGameUIFuncs->GetVideoModes(&modes, &count);
    std::set<std::pair<int, int>> seen;
    std::string list = "[";
    for (int i = 0; i < count; i++) {
        if (modes[i].iWidth < 640 || modes[i].iHeight < 480) continue;
        if (!seen.insert({modes[i].iWidth, modes[i].iHeight}).second) continue;
        if (list.size() > 1) list += ',';
        list += "[" + std::to_string(modes[i].iWidth) + "," + std::to_string(modes[i].iHeight) + "]";
    }
    list += "]";
    VideoState v = current_video();
    std::string json = "{\"modes\":" + list + ",\"width\":" + std::to_string(v.w) + ",\"height\":" + std::to_string(v.h) +
                       ",\"windowed\":" + json_bool(v.windowed != 0) + ",\"lowDetail\":" + json_bool(v.low != 0) + "}";
    reply->Resolve(json.c_str());
}

// "width height windowed lowDetail"; restarts the engine like the vgui dialog, only on a change
void on_apply_video(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    int w = 0, h = 0, windowed = 0, low = 0;
    if (!arg || sscanf(arg, "%d %d %d %d", &w, &h, &windowed, &low) != 4) return reply->Reject(400);

    vmode_t* modes = nullptr;
    int count = 0;
    bool known = false;
    g_pGameUIFuncs->GetVideoModes(&modes, &count);
    for (int i = 0; i < count && !known; i++) known = modes[i].iWidth == w && modes[i].iHeight == h;
    if (!known) return reply->Reject(400);

    VideoState v = current_video();
    windowed = windowed ? 1 : 0;
    low = low ? 1 : 0;
    if (v.w == w && v.h == h && (v.windowed != 0) == (windowed != 0) && (v.low != 0) == (low != 0))
        return reply->Resolve("{\"restart\":false}");

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "_setvideomode %d %d %d\n", w, h, v.bpp > 16 ? 32 : 16);
    engine->pfnClientCmd(cmd);
    snprintf(cmd, sizeof(cmd), "_setrenderer gl %s\n", windowed ? "windowed" : "fullscreen");
    engine->pfnClientCmd(cmd);
    snprintf(cmd, sizeof(cmd), "_set_vid_level %d\n", low);
    engine->pfnClientCmd(cmd);
    reply->Resolve("{\"restart\":true}");
    engine->pfnClientCmd("fmod stop\n");
    engine->pfnClientCmd("_restart\n");
}

// sections of gfx/shell/kb_act.lst with localized captions, the current key of every
// binding and the defaults of kb_def.lst
void on_key_bindings(void*, const char*, const char*, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);

    std::vector<std::string> act = list_tokens("gfx/shell/kb_act.lst");
    std::string sections = "[";
    bool open = false;
    for (size_t i = 0; i + 1 < act.size(); i += 2) {
        const std::string& binding = act[i];
        const std::string& caption = act[i + 1];
        if (caption.empty() || caption[0] == '=') continue;
        if (_stricmp(binding.c_str(), "blank") == 0) {
            if (open) sections += "]},";
            sections += "{\"title\":" + json_string(localized(caption)) + ",\"items\":[";
            open = true;
            continue;
        }
        if (!open) { sections += "{\"title\":\"\",\"items\":["; open = true; }
        if (sections.back() != '[') sections += ',';
        sections += "{\"binding\":" + json_string(binding) + ",\"label\":" + json_string(localized(caption)) + "}";
    }
    if (open) sections += "]}";
    sections += "]";

    std::string keys = "{";
    for (int k = 0; k < 256; k++) {
        const char* name = g_pGameUIFuncs->Key_NameForKey(k);
        const char* binding = g_pGameUIFuncs->Key_BindingForKey(k);
        if (!name || !name[0] || !binding || !binding[0]) continue;
        if (keys.size() > 1) keys += ',';
        keys += json_string(name) + ":" + json_string(binding);
    }
    keys += "}";

    std::vector<std::string> def = list_tokens("gfx/shell/kb_def.lst");
    std::string defaults = "[";
    for (size_t i = 0; i + 1 < def.size(); i += 2) {
        if (defaults.size() > 1) defaults += ',';
        defaults += "[" + json_string(def[i]) + "," + json_string(def[i + 1]) + "]";
    }
    defaults += "]";

    std::string json = "{\"sections\":" + sections + ",\"keys\":" + keys + ",\"defaults\":" + defaults + "}";
    reply->Resolve(json.c_str());
}

bool safe_bind_text(const std::string& s) {
    if (s.empty() || s.size() > 63) return false;
    for (unsigned char c : s)
        if (c <= ' ' || c == '"' || c == ';' || c == '\\' || c >= 0x7f) return false;
    return true;
}

// "key\tbinding" binds, "key\t" unbinds; the binding must be one of kb_act.lst's actions
void on_bind_key(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    std::string a = arg ? arg : "";
    size_t tab = a.find('\t');
    if (tab == std::string::npos) return reply->Reject(400);
    std::string key = a.substr(0, tab), binding = a.substr(tab + 1);
    if (!safe_bind_text(key) || g_pGameUIFuncs->Key_KeyStringToKeyNum(key.c_str()) < 0) return reply->Reject(400);

    if (binding.empty()) {
        engine->pfnClientCmd(const_cast<char*>(("unbind \"" + key + "\"\n").c_str()));
        return reply->Resolve("true");
    }

    bool known = false;
    std::vector<std::string> act = list_tokens("gfx/shell/kb_act.lst");
    for (size_t i = 0; i + 1 < act.size() && !known; i += 2) known = _stricmp(act[i].c_str(), binding.c_str()) == 0;
    if (!known || !safe_bind_text(binding)) return reply->Reject(400);

    engine->pfnClientCmd(const_cast<char*>(("bind \"" + key + "\" \"" + binding + "\"\n").c_str()));
    reply->Resolve("true");
}

// ---- "New game" page: the map list and a listen server start (the vgui CreateServer dialog) ----

void collect_maps(const char* path_id, std::set<std::string>& out) {
    const char* filter = ModInfo().GetMPFilter();
    FileFindHandle_t handle = 0;
    for (const char* f = g_pFullFileSystem->FindFirst("maps/*.bsp", &handle, path_id); f; f = g_pFullFileSystem->FindNext(handle)) {
        std::string name = f;
        size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos) name = name.substr(slash + 1);
        if (name.size() > 4 && _stricmp(name.c_str() + name.size() - 4, ".bsp") == 0) name.resize(name.size() - 4);
        if (name.empty() || (filter && filter[0] && name.find(filter) != std::string::npos)) continue;
        for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        out.insert(name);
    }
    g_pFullFileSystem->FindClose(handle);
}

std::set<std::string> all_maps() {
    std::set<std::string> maps;
    collect_maps("GAME", maps);
    if (ModInfo().GetFallbackDir()[0]) collect_maps("GAME_FALLBACK", maps);
    collect_maps("GAMEDOWNLOAD", maps);
    return maps;
}

void on_maps(void*, const char*, const char*, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    std::string list = "[";
    for (const std::string& m : all_maps()) {
        if (list.size() > 1) list += ',';
        list += json_string(m);
    }
    list += "]";
    std::string json = "{\"maps\":" + list + "}";
    reply->Resolve(json.c_str());
}

// cvars the page may set on the new server: settings.scr's rules and the CS bot options
const std::set<std::string> kServerCvars = {
    "mp_timelimit", "mp_winlimit", "mp_maxrounds", "mp_roundtime", "mp_freezetime", "mp_buytime", "mp_startmoney",
    "mp_footsteps", "mp_forcecamera", "mp_fadetoblack", "mp_friendlyfire", "mp_tkpunish", "mp_autokick",
    "mp_hostagepenalty", "mp_flashlight", "mp_autoteambalance", "mp_limitteams",
    "bot_quota", "bot_difficulty", "bot_join_after_player", "bot_allow_rogues", "bot_allow_pistols", "bot_allow_shotguns",
    "bot_allow_sub_machine_guns", "bot_allow_machine_guns", "bot_allow_rifles", "bot_allow_snipers", "bot_allow_grenades",
    "bot_allow_shield", "bot_join_team", "bot_chatter", "bot_defer_to_human"};

// Commands run once the new map is up (server.cfg would override them before): left the old
// level (or was not in one), entered the new one, and let a few frames pass.
struct PendingServerSettings {
    std::string commands;
    bool left = false;
    int frames = 0;
};
std::optional<PendingServerSettings> g_pending_server;

bool safe_value(const std::string& s) {
    if (s.size() > 63) return false;
    for (unsigned char c : s)
        if (c < ' ' || c == '"' || c == ';' || c == '\\') return false;
    return true;
}

// lines "key\tvalue": map, maxplayers, hostname, sv_password and kServerCvars
void on_start_game(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    std::string map, hostname = "Counter-Strike", password;
    int maxplayers = 16;
    std::string cvars;

    std::string a = arg ? arg : "";
    size_t pos = 0;
    while (pos < a.size()) {
        size_t end = a.find('\n', pos);
        if (end == std::string::npos) end = a.size();
        std::string line = a.substr(pos, end - pos);
        pos = end + 1;
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string key = line.substr(0, tab), value = line.substr(tab + 1);
        if (!safe_value(value)) return reply->Reject(400);
        if (key == "map") map = value;
        else if (key == "maxplayers") maxplayers = std::clamp(atoi(value.c_str()), 2, 32);
        else if (key == "hostname") hostname = value;
        else if (key == "sv_password") password = value;
        else if (kServerCvars.count(key)) cvars += key + " \"" + value + "\"\n";
        else return reply->Reject(400);
    }

    std::set<std::string> maps = all_maps();
    if (map.empty() || map == "*") {
        if (maps.empty()) return reply->Reject(404);
        auto it = maps.begin();
        std::advance(it, engine->pfnRandomLong(0, static_cast<int>(maps.size()) - 1));
        map = *it;
    } else if (!maps.count(map)) {
        return reply->Reject(404);
    }

    std::string cmd = "disconnect\nsv_lan 1\nsetmaster enable\nmaxplayers " + std::to_string(maxplayers) +
                      "\nsv_password \"" + password + "\"\nhostname \"" + hostname + "\"\n" + cvars +
                      "cd fadeout\nmap " + map + "\n";
    engine->pfnClientCmd(const_cast<char*>(cmd.c_str()));

    g_pending_server = PendingServerSettings{cvars + "mp_autoteambalance 0\nmp_limitteams 0\n", !GameUI().IsInLevel(), 0};
    std::string json = "{\"map\":" + json_string(map) + "}";
    reply->Resolve(json.c_str());
}

void run_pending_server_settings() {
    if (!g_pending_server) return;
    PendingServerSettings& p = *g_pending_server;
    bool in_level = GameUI().IsInLevel();
    if (!p.left) {
        p.left = !in_level;
        return;
    }
    if (!in_level || ++p.frames < 30) return;
    engine->pfnClientCmd(const_cast<char*>(p.commands.c_str()));
    g_pending_server.reset();
}

// "x y w h move demo" in browser pixels; w 0 hides the weapon preview
void on_game_preview(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    int x = 0, y = 0, w = 0, h = 0, move = 3, demo = 0;
    if (!arg || sscanf(arg, "%d %d %d %d %d %d", &x, &y, &w, &h, &move, &demo) < 4) return reply->Reject(400);
    CMainMenuBrowser* browser = BasePanel() ? BasePanel()->GetMainMenuBrowser() : nullptr;
    if (!browser) return reply->Reject(404);
    browser->SetGamePreview(x, y, w, h, move, demo);
    reply->Resolve("true");
}

void on_options(void*, const char*, const char* arg, const char* frame_url, IBrowserApiReply* reply) {
    if (!trusted(frame_url)) return reply->Reject(403);
    g_options_tab = arg ? arg : "";
    reply->Resolve("true");
}

ncl::IBrowserApi* find_api() {
    HMODULE m = GetModuleHandleA("chromehtml.dll");
    if (!m) m = LoadLibraryA("chromehtml.dll");
    auto create = m ? reinterpret_cast<CreateInterfaceFn>(GetProcAddress(m, "CreateInterface")) : nullptr;
    // The SDK's CreateInterfaceFn returns IBaseInterface*; chromehtml.dll returns the IBrowserApi object.
    return create ? reinterpret_cast<ncl::IBrowserApi*>(create(NCL_BROWSER_API_VERSION, nullptr)) : nullptr;
}

}  // namespace

void BrowserApi_Init() {
    g_api = find_api();
    if (!g_api) return;  // an old chromehtml.dll: pages simply lack these methods
    g_api->RegisterMethod("getRules", &on_rules, nullptr);
    g_api->RegisterMethod("getFavoriteServers", &on_list, reinterpret_cast<void*>(ListKind::Favorites));
    g_api->RegisterMethod("getHistoryServers", &on_list, reinterpret_cast<void*>(ListKind::History));
    g_api->RegisterMethod("getInternetServers", &on_list, reinterpret_cast<void*>(ListKind::Internet));
    g_api->RegisterMethod("addFavoriteServer", &on_favorite, reinterpret_cast<void*>(1));
    g_api->RegisterMethod("removeFavoriteServer", &on_favorite, nullptr);
    g_api->RegisterMethod("openOptionsDialog", &on_options, nullptr);
    g_api->RegisterMethod("gameUiCommand", &on_menu_command, nullptr);
    g_api->RegisterMethod("openExternal", &on_open_external, nullptr);
    g_api->RegisterMethod("getVideoState", &on_video_state, nullptr);
    g_api->RegisterMethod("applyVideo", &on_apply_video, nullptr);
    g_api->RegisterMethod("getKeyBindings", &on_key_bindings, nullptr);
    g_api->RegisterMethod("bindKey", &on_bind_key, nullptr);
    g_api->RegisterMethod("setGamePreview", &on_game_preview, nullptr);
    g_api->RegisterMethod("getMaps", &on_maps, nullptr);
    g_api->RegisterMethod("startGame", &on_start_game, nullptr);
    g_api->RegisterMethod("getAccountToken", &on_account_token, nullptr);
    g_api->RegisterMethod("setAccountToken", &on_set_account_token, nullptr);
    g_api->RegisterMethod("siteApi", &on_site_api, nullptr);
    g_api->RegisterMethod("launchGame", &on_launch_game, nullptr);
    g_api->RegisterMethod("setGameRect", &on_set_game_rect, nullptr);
    g_api->RegisterMethod("closeGame", &on_close_game, nullptr);
    g_api->RegisterMethod("gameInput", &on_game_input, nullptr);
    g_api->RegisterMethod("gameRunning", &on_game_running, nullptr);
}

void BrowserApi_Shutdown() {
    if (g_game.process) TerminateProcess(g_game.process, 0);
    if (!g_api) return;
    for (const char* m : {"getRules", "getFavoriteServers", "getHistoryServers", "getInternetServers",
                          "addFavoriteServer", "removeFavoriteServer", "openOptionsDialog", "gameUiCommand", "openExternal", "getVideoState",
                          "applyVideo", "getKeyBindings", "bindKey", "setGamePreview", "getMaps", "startGame", "getAccountToken", "setAccountToken", "siteApi", "launchGame", "setGameRect", "closeGame", "gameRunning", "gameInput"})
        g_api->UnregisterMethod(m);
    g_api = nullptr;
}

void BrowserApi_RunFrame() {
    run_game_frame();
    std::vector<ListQuery*> expired;
    for (ListQuery* q : g_lists)
        if (q->expired()) expired.push_back(q);
    for (ListQuery* q : expired) q->finish(true);

    run_pending_server_settings();

    if (g_api) {
        for (auto& [list, items] : g_batches) {
            if (items.empty()) continue;
            std::string detail = "{\"list\":\"" + list + "\",\"items\":[";
            for (size_t i = 0; i < items.size(); ++i) detail += (i ? "," : "") + items[i];
            g_api->DispatchEvent("nextclient.matchmaking", "servers", (detail + "]}").c_str());
        }
    }
    g_batches.clear();

    if (!g_menu_command.empty()) {
        std::string c;
        c.swap(g_menu_command);
        run_menu_command(c);
    }

    if (g_options_tab.empty()) return;
    std::string tab;
    tab.swap(g_options_tab);
    BasePanel()->OnOpenOptionsDialog(tab.c_str());
}

void BrowserApi_ConsolePrint(const std::string& utf8_text) {
    if (g_api) g_api->DispatchEvent("nextclient.console", "message", ("{\"text\":" + json_string(utf8_text) + "}").c_str());
}
