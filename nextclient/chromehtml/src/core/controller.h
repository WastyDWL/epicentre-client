// ChromeHTML_Controller_001 implemented on the Engine (CEF 150): what hw.dll and the vgui2::HTML
// panels talk to. One NcSerializer per panel; its address is the panel's key in the Engine.
#pragma once

#include "common/valve_html.h"

namespace nc {

class NcSerializer final : public IHTMLSerializer {
public:
    explicit NcSerializer(IHTMLResponses_HL1* target) : target_(target) {}
    IHTMLResponses_HL1* target() const { return target_; }

    IHTMLResponses* GetResponseTarget() override;
    int BrowserGetIndex() override;
    void BrowserPosition(int x, int y) override;
    void PostURL(const char* url, const char* post_data) override;
    void BrowserErrorStrings(const char* title, const char* header, const char* cache_miss, const char* bad_url,
                             const char* connection, const char* proxy, const char* unknown) override;
    void StopLoad() override;
    void Reload() override;
    void GoBack() override;
    void GoForward() override;
    void MouseDown(int code) override;
    void MouseUp(int code) override;
    void MouseWheel(int delta) override;
    void MouseMove(int x, int y) override;
    void MouseDoubleClick(int code) override;
    void BrowserSize(int wide, int tall) override;
    void KeyUp(int key, int mods) override;
    void KeyDown(int key, int mods) override;
    void KeyChar(int unichar) override;
    void RunJavaScript(const char* script) override;
    void SetHorizontalScroll(int scroll) override {}
    void SetVerticalScroll(int scroll) override {}
    void SetFocus(bool focus) override;
    void AddHeader(const char* name, const char* value) override;
    void NeedsPaintResponse(int texture_id) override;
    void StartRequestResponse(bool allow) override;
    void RequestBrowserSizes() override;
    void JSDialogResponse(int result) override;
    void GetLinkAtPosition(int x, int y) override;

private:
    IHTMLResponses_HL1* target_;
};

// The opaque IHTMLResponses* handed to CreateBrowser/RemoveBrowser is the serializer itself.
NcSerializer* serializer_from_target(IHTMLResponses* target);

class NcController final : public IHTMLChromeController {
public:
    bool Init(const char* html_cache_dir, const char* cookie_path) override;
    void Shutdown() override;
    bool RunFrame() override;
    void SetWebCookie(const char* host, const char* key, const char* value, const char* path,
                      RTime32 expires) override;
    void GetWebCookiesForURL(CUtlString*, const char*, const char*) override {}
    void SetClientBuildID(uint64_t) override {}
    bool BHasPendingMessages() override { return false; }
    void CreateBrowser(IHTMLResponses* browser, bool popup, const char* user_agent) override;
    void RemoveBrowser(IHTMLResponses* browser) override;
    void WakeThread() override {}
    HTMLCommandBuffer_t* GetFreeCommandBuffer(EHTMLCommands, int) override { return nullptr; }
    void PushCommand(HTMLCommandBuffer_t*) override {}
    void SetCefThreadTargetFrameRate(uint32_t fps) override;
    IHTMLSerializer* CreateSerializer(IHTMLResponses_HL1* target) override;
};

}  // namespace nc
