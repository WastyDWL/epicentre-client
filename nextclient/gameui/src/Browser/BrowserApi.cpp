#include "BrowserApi.h"

#include <windows.h>

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <GameServerHelpers.h>
#include <GameUi.h>
#include <TaskRun.h>
#include <chromehtml/ncl_browser_api.h>
#include <nitro_utils/net_utils.h>
#include <steam/steam_api.h>

#include "BasePanel.h"

namespace {

using ncl::IBrowserApiReply;

ncl::IBrowserApi* g_api = nullptr;
std::string g_options_tab;

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
}

void BrowserApi_Shutdown() {
    if (!g_api) return;
    for (const char* m : {"getRules", "getFavoriteServers", "getHistoryServers", "getInternetServers",
                          "addFavoriteServer", "removeFavoriteServer", "openOptionsDialog"})
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

    if (g_options_tab.empty()) return;
    std::string tab;
    tab.swap(g_options_tab);
    BasePanel()->OnOpenOptionsDialog(tab.c_str());
}

void BrowserApi_ConsolePrint(const std::string& utf8_text) {
    if (g_api) g_api->DispatchEvent("nextclient.console", "message", ("{\"text\":" + json_string(utf8_text) + "}").c_str());
}
