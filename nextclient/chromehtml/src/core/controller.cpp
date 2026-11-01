#include "core/controller.h"

#include <windows.h>

#include <string>

#include "common/log.h"
#include "core/engine.h"

namespace nc {

namespace {

// Lazily created: vgui2::HTML calls AddHeader and BrowserSize between CreateSerializer and
// CreateBrowser, and those must not be lost. Nothing happens while CEF is not running.
Browser* browser(NcSerializer* s) { return Engine::get().lazy_for_panel(s, s->target()); }

// <game>\browser — CEF runtime in cef\, cache, logs and browser.ini.
std::wstring browser_dir() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&browser_dir), &self);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(self, path, MAX_PATH);
    *wcsrchr(path, L'\\') = 0;
    return std::wstring(path) + L"\\browser";
}

}  // namespace

IHTMLResponses* NcSerializer::GetResponseTarget() { return reinterpret_cast<IHTMLResponses*>(this); }

NcSerializer* serializer_from_target(IHTMLResponses* target) { return reinterpret_cast<NcSerializer*>(target); }

int NcSerializer::BrowserGetIndex() {
    Browser* b = Engine::get().find(this);
    return b ? b->index() : -1;
}

#define WITH_BROWSER(call)              \
    do {                                \
        if (Browser* b = browser(this)) \
            b->call;                    \
    } while (0)

void NcSerializer::BrowserPosition(int x, int y) { WITH_BROWSER(set_position(x, y)); }
void NcSerializer::PostURL(const char* url, const char* post) { WITH_BROWSER(post_url(url, post)); }
void NcSerializer::BrowserErrorStrings(const char* title, const char* header, const char* cache_miss,
                                       const char* bad_url, const char* connection, const char* proxy,
                                       const char* unknown) {
    auto s = [](const char* p) { return std::string(p ? p : ""); };
    WITH_BROWSER(set_error_strings({s(title), s(header), s(cache_miss), s(bad_url), s(connection), s(proxy), s(unknown)}));
}
void NcSerializer::StopLoad() { WITH_BROWSER(stop_load()); }
void NcSerializer::Reload() { WITH_BROWSER(reload()); }
void NcSerializer::GoBack() { WITH_BROWSER(go_back()); }
void NcSerializer::GoForward() { WITH_BROWSER(go_forward()); }
void NcSerializer::MouseDown(int code) { WITH_BROWSER(mouse_down(code)); }
void NcSerializer::MouseUp(int code) { WITH_BROWSER(mouse_up(code)); }
void NcSerializer::MouseWheel(int delta) { WITH_BROWSER(mouse_wheel(delta)); }
void NcSerializer::MouseMove(int x, int y) { WITH_BROWSER(mouse_move(x, y)); }
void NcSerializer::MouseDoubleClick(int code) { WITH_BROWSER(mouse_double_click(code)); }
void NcSerializer::BrowserSize(int w, int h) { WITH_BROWSER(set_size(w, h)); }
void NcSerializer::KeyUp(int key, int mods) { WITH_BROWSER(key_up(key, mods)); }
void NcSerializer::KeyDown(int key, int mods) { WITH_BROWSER(key_down(key, mods)); }
void NcSerializer::KeyChar(int ch) { WITH_BROWSER(key_char(ch)); }
void NcSerializer::RunJavaScript(const char* js) { WITH_BROWSER(run_js(js)); }
void NcSerializer::SetFocus(bool focus) { WITH_BROWSER(set_focus(focus)); }
void NcSerializer::AddHeader(const char* name, const char* value) { WITH_BROWSER(add_header(name, value)); }
void NcSerializer::NeedsPaintResponse(int texture_id) { WITH_BROWSER(needs_paint_response(texture_id)); }
void NcSerializer::StartRequestResponse(bool allow) { WITH_BROWSER(start_request_response(allow)); }
void NcSerializer::RequestBrowserSizes() { WITH_BROWSER(request_sizes()); }
void NcSerializer::JSDialogResponse(int result) { WITH_BROWSER(js_dialog_response(result)); }
void NcSerializer::GetLinkAtPosition(int x, int y) { WITH_BROWSER(get_link_at(x, y)); }

bool NcController::Init(const char*, const char*) {
    std::wstring dir = browser_dir();
    log_open((dir + L"\\browser.log").c_str(), true);
    Config cfg = load_config(dir + L"\\browser.ini", L"");
    cfg.enabled = true;  // there is no other browser to fall back to
    bool ok = Engine::get().init({dir, cfg});
    log(ok ? "chromehtml: CEF ready" : "chromehtml: CEF failed to start; HTML panels stay empty");
    return ok;
}

void NcController::Shutdown() { Engine::get().shutdown(); }

bool NcController::RunFrame() {
    if (Engine::get().running()) Engine::get().run_frame();
    return true;
}

void NcController::SetWebCookie(const char* host, const char* key, const char* value, const char* path,
                                RTime32 expires) {
    Engine::get().set_cookie(host, key, value, path, expires);
}

void NcController::CreateBrowser(IHTMLResponses* target, bool, const char* user_agent) {
    if (NcSerializer* s = serializer_from_target(target))
        Engine::get().create_for_panel(s, s->target(), user_agent ? user_agent : "");
}

void NcController::RemoveBrowser(IHTMLResponses* target) {
    if (NcSerializer* s = serializer_from_target(target)) Engine::get().remove(s);
}

void NcController::SetCefThreadTargetFrameRate(uint32_t fps) { Engine::get().set_frame_rate(static_cast<int>(fps)); }

IHTMLSerializer* NcController::CreateSerializer(IHTMLResponses_HL1* target) {
    // Never freed: vgui2::HTML drops its pointer after RemoveBrowser but may not own it.
    return target ? new NcSerializer(target) : nullptr;
}

}  // namespace nc
