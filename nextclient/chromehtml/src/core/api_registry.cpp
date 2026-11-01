#include "core/api_registry.h"

#include <memory>

#include "core/a2s.h"

namespace nc {

namespace {

constexpr char kMenuOrigin[] = "ncl://menu/";

// Lives until the handler answers; deletes itself after the first Resolve/Reject.
class Reply final : public ncl::IBrowserApiReply {
public:
    explicit Reply(std::function<void(ApiResult)> done) : done_(std::move(done)) {}
    void Resolve(const char* json) override { finish({200, json && *json ? json : "null"}); }
    void Reject(int status) override { finish({status >= 400 ? status : 500, "null"}); }

private:
    void finish(ApiResult r) {
        auto done = std::move(done_);
        delete this;
        if (done) done(std::move(r));
    }
    std::function<void(ApiResult)> done_;
};

}  // namespace

void ApiRegistry::RegisterMethod(const char* method, ncl::BrowserApiHandler handler, void* context) {
    if (method && handler) methods_[method] = {handler, context};
}

void ApiRegistry::UnregisterMethod(const char* method) {
    if (method) methods_.erase(method);
}

void ApiRegistry::DispatchEvent(const char* target, const char* type, const char* detail_json) {
    if (!target || !type) return;
    scripts_.push_back(dispatch_script(target, type, detail_json && *detail_json ? detail_json : "null"));
}

bool ApiRegistry::IsTrusted(const char* frame_url) {
    if (!frame_url) return false;
    std::string url = frame_url;
    if (url.rfind(kMenuOrigin, 0) == 0) return true;  // the client's own menu
    return domains_ && domains_->is_accepted(url);
}

void ApiRegistry::dispatch(const ApiCall& call, const std::string& frame_url, const Domains& domains, GameApi& game,
                           std::function<void(ApiResult)> done) {
    auto it = methods_.find(call.method);
    if (it != methods_.end()) {
        it->second.handler(it->second.context, call.method.c_str(), call.arg.c_str(), frame_url.c_str(),
                           new Reply(std::move(done)));
        return;
    }
    // Built-in server queries (UDP A2S) when no module provides matchmaking.
    if (post_ && (call.method == "getServerInfo" || call.method == "getPlayersInfo")) {
        auto post = post_;
        auto finish = [post, done = std::move(done)](std::optional<std::string> json) {
            post([done, json] { done(json ? ApiResult{200, *json} : ApiResult{504, "null"}); });
        };
        if (call.method == "getServerInfo") a2s::query_info(call.arg, finish);
        else a2s::query_players(call.arg, finish);
        return;
    }
    const Domains* saved = domains_;
    if (!domains_) domains_ = &domains;
    bool trusted = IsTrusted(frame_url.c_str());
    domains_ = saved;
    done(route_api(call, trusted, game));
}

std::vector<std::string> ApiRegistry::take_scripts() {
    std::vector<std::string> out;
    out.swap(scripts_);
    return out;
}

}  // namespace nc
