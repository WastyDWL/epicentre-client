// NCL_BrowserApi_001 implementation: window.nextclient methods registered by client modules,
// falling back to chromehtml's built-in ones (api_router), plus queued game events.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "chromehtml/ncl_browser_api.h"
#include "core/api_router.h"

namespace nc {

class ApiRegistry final : public ncl::IBrowserApi {
public:
    void RegisterMethod(const char* method, ncl::BrowserApiHandler handler, void* context) override;
    void UnregisterMethod(const char* method) override;
    void DispatchEvent(const char* target, const char* type, const char* detail_json) override;
    bool IsTrusted(const char* frame_url) override;

    void set_domains(const Domains* domains) { domains_ = domains; }
    // Main thread. `done` runs once, now or when a registered handler replies.
    void dispatch(const ApiCall& call, const std::string& frame_url, const Domains& domains, GameApi& game,
                  std::function<void(ApiResult)> done);
    // Event scripts queued since the last call, to run in every frame.
    std::vector<std::string> take_scripts();

private:
    struct Entry {
        ncl::BrowserApiHandler handler;
        void* context;
    };
    std::map<std::string, Entry> methods_;
    std::vector<std::string> scripts_;
    const Domains* domains_ = nullptr;
};

}  // namespace nc
