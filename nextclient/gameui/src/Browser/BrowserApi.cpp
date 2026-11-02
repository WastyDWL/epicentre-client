#include "BrowserApi.h"

#include <windows.h>
#include <shellapi.h>

#include <chrono>
#include <map>
#include <set>
#include <string>
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
}

void BrowserApi_Shutdown() {
    if (!g_api) return;
    for (const char* m : {"getRules", "getFavoriteServers", "getHistoryServers", "getInternetServers",
                          "addFavoriteServer", "removeFavoriteServer", "openOptionsDialog", "gameUiCommand", "openExternal", "getVideoState",
                          "applyVideo", "getKeyBindings", "bindKey", "setGamePreview"})
        g_api->UnregisterMethod(m);
    g_api = nullptr;
}

void BrowserApi_RunFrame() {
    std::vector<ListQuery*> expired;
    for (ListQuery* q : g_lists)
        if (q->expired()) expired.push_back(q);
    for (ListQuery* q : expired) q->finish(true);

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
