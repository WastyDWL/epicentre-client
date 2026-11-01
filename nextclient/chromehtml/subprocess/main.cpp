// Chromium helper process. In the renderer it installs window.nextclient into every JS context.
#include <windows.h>

#include "common/ncb_scheme.h"
#include "core/js_api.h"
#include <mutex>
#include <set>

#include "include/cef_app.h"

namespace {

class SubprocessApp final : public CefApp, public CefRenderProcessHandler {
public:
    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override {
        nc::register_ncb_scheme(registrar);
    }
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    void OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> extra) override {
        if (extra && extra->GetBool("mainMenu")) {
            std::lock_guard lock(mutex_);
            main_menu_.insert(browser->GetIdentifier());
        }
    }
    void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override {
        std::lock_guard lock(mutex_);
        main_menu_.erase(browser->GetIdentifier());
    }

    void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override {
        CefRefPtr<CefV8Value> result;
        CefRefPtr<CefV8Exception> error;
        context->Eval(nc::api_script(), "ncb://api/bootstrap.js", 1, result, error);
        bool main_menu;
        {
            std::lock_guard lock(mutex_);
            main_menu = main_menu_.count(browser->GetIdentifier()) != 0;
        }
        if (main_menu && frame->IsMain() && nc::wants_back_button(frame->GetURL().ToString()))
            context->Eval(nc::back_button_script(), "ncb://api/back.js", 1, result, error);
    }

private:
    std::mutex mutex_;
    std::set<int> main_menu_;  // browsers of the main-menu panel

    IMPLEMENT_REFCOUNTING(SubprocessApp);
};

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    CefMainArgs args(instance);
    CefRefPtr<CefApp> app = new SubprocessApp();
    return CefExecuteProcess(args, app, nullptr);
}
