#include <string>
#include <vector>

#include "core/api_registry.h"
#include "testing.h"

using namespace nc;

namespace {

struct Recorded {
    std::string method, arg, frame;
    ncl::IBrowserApiReply* reply = nullptr;
};

void record(void* ctx, const char* method, const char* arg, const char* frame_url, ncl::IBrowserApiReply* reply) {
    auto* r = static_cast<Recorded*>(ctx);
    r->method = method;
    r->arg = arg;
    r->frame = frame_url;
    r->reply = reply;
}

struct NoGame : GameApi {
    std::optional<std::string> cvar(const std::string&) override { return std::string("builtin"); }
    void client_cmd(const std::string&) override {}
    std::optional<ConnectedHost> host() override { return std::nullopt; }
    bool gameui_shown() override { return false; }
};

}  // namespace

TEST(registry_routes_to_registered_handler) {
    ApiRegistry reg;
    Recorded rec;
    reg.RegisterMethod("getServerInfo", &record, &rec);
    NoGame game;
    Domains d;
    std::optional<ApiResult> result;
    reg.dispatch({"getServerInfo", "1.2.3.4:27015"}, "https://x.ru/", d, game, [&](ApiResult r) { result = r; });
    CHECK_EQ(rec.method, std::string("getServerInfo"));
    CHECK_EQ(rec.arg, std::string("1.2.3.4:27015"));
    CHECK_EQ(rec.frame, std::string("https://x.ru/"));
    CHECK(!result.has_value());  // asynchronous: nothing until the handler replies
    rec.reply->Resolve("{\"map\":\"de_dust2\"}");
    CHECK(result.has_value());
    CHECK_EQ(result->status, 200);
    CHECK_EQ(result->json, std::string("{\"map\":\"de_dust2\"}"));
}

TEST(registry_reject_status) {
    ApiRegistry reg;
    Recorded rec;
    reg.RegisterMethod("getRules", &record, &rec);
    NoGame game;
    Domains d;
    std::optional<ApiResult> result;
    reg.dispatch({"getRules", ""}, "", d, game, [&](ApiResult r) { result = r; });
    rec.reply->Reject(500);
    CHECK_EQ(result->status, 500);
}

TEST(registry_falls_back_to_builtin) {
    ApiRegistry reg;
    NoGame game;
    Domains d;
    std::optional<ApiResult> result;
    reg.dispatch({"getCvarValue", "name"}, "", d, game, [&](ApiResult r) { result = r; });
    CHECK_EQ(result->json, std::string("\"builtin\""));
}

TEST(registry_unregister_restores_builtin) {
    ApiRegistry reg;
    Recorded rec;
    reg.RegisterMethod("getCvarValue", &record, &rec);
    reg.UnregisterMethod("getCvarValue");
    NoGame game;
    Domains d;
    std::optional<ApiResult> result;
    reg.dispatch({"getCvarValue", "name"}, "", d, game, [&](ApiResult r) { result = r; });
    CHECK(rec.reply == nullptr);
    CHECK_EQ(result->json, std::string("\"builtin\""));
}

TEST(registry_trust_menu_and_domains) {
    ApiRegistry reg;
    Domains d;
    d.load_from_string("epicentre-cs.ru");
    reg.set_domains(&d);
    CHECK(reg.IsTrusted("ncl://menu/index.html"));
    CHECK(reg.IsTrusted("https://epicentre-cs.ru/shop"));
    CHECK(!reg.IsTrusted("https://evil.example/"));
    CHECK(!reg.IsTrusted("ncl://menu.evil/"));
}

TEST(registry_events_queued_as_scripts) {
    ApiRegistry reg;
    reg.DispatchEvent("nextclient.console", "message", "{\"text\":\"hi\"}");
    std::vector<std::string> scripts = reg.take_scripts();
    CHECK_EQ(scripts.size(), (size_t)1);
    CHECK_EQ(scripts[0], dispatch_script("nextclient.console", "message", "{\"text\":\"hi\"}"));
    CHECK(reg.take_scripts().empty());
}
