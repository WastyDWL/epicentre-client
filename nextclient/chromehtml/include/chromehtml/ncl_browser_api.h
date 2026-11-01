// NCL_BrowserApi_001: how client modules (GameUI, engine_mini) extend window.nextclient in the
// Chromium-based chromehtml.dll. Obtain it with chromehtml.dll's CreateInterface. All methods and
// all handler calls happen on the game's main thread.
#pragma once

#define NCL_BROWSER_API_VERSION "NCL_BrowserApi_001"

namespace ncl {

// Answers one call. Exactly one of Resolve/Reject, once; may be called later (asynchronous work),
// still on the main thread. The object stays valid until then.
class IBrowserApiReply {
public:
    virtual void Resolve(const char* json) = 0;  // JSON value: "null", "true", "{...}", "\"text\""
    virtual void Reject(int status) = 0;         // HTTP-like: 403 not allowed, 404 unknown, 500 failed
};

// method: e.g. "getServerInfo"; arg: the call's argument as text ("" when none);
// frame_url: URL of the calling frame, for trust decisions (see IBrowserApi::IsTrusted).
using BrowserApiHandler = void (*)(void* context, const char* method, const char* arg, const char* frame_url,
                                   IBrowserApiReply* reply);

class IBrowserApi {
public:
    // Registers (or replaces) the handler for nextclient method `method`.
    virtual void RegisterMethod(const char* method, BrowserApiHandler handler, void* context) = 0;
    virtual void UnregisterMethod(const char* method) = 0;
    // Fires `type` on window.<target> ("nextclient", "nextclient.console", ...) in every page.
    virtual void DispatchEvent(const char* target, const char* type, const char* detail_json) = 0;
    // True for the built-in menu and domains listed in platform/accepted_domains.txt.
    virtual bool IsTrusted(const char* frame_url) = 0;
};

}  // namespace ncl
