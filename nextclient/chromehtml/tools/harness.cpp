// chromehtml_harness <dir with chromehtml.dll> <url> <out.bmp> [w h] [settle_ms]
// Drives chromehtml.dll exactly like hw.dll and a vgui2::HTML panel do, without the game.
#include <windows.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "common/valve_html.h"

namespace {

struct Panel final : IHTMLResponses_HL1 {
    IHTMLSerializer* s = nullptr;
    bool ready = false, finished = false;
    int paints = 0, w = 0, h = 0;
    std::vector<unsigned char> frame;
    std::string title, url;

    void BrowserReady() override { ready = true; }
    void BrowserNeedsPaint(int tex, int wide, int tall, const unsigned char* bgra, int, int, int, int, int, int,
                           const unsigned char*) override {
        w = wide;
        h = tall;
        frame.assign(bgra, bgra + static_cast<size_t>(wide) * tall * 4);
        ++paints;
        s->NeedsPaintResponse(tex);
    }
    void BrowserStartRequest(const char*, const char*, const char*, bool) override { s->StartRequestResponse(true); }
    void BrowserURLChanged(const char* u, const char*, bool) override { url = u; }
    void BrowserFinishedRequest(const char* u, const char* t) override {
        url = u;
        if (t && *t) title = t;
        finished = true;
    }
    void BrowserShowPopup() override {}
    void BrowserHidePopup() override {}
    void BrowserSizePopup(int, int, int, int) override {}
    void BrowserHorizontalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserVerticalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserGetZoomResponse(float) override {}
    void BrowserCanGoBackandForward(bool, bool) override {}
    void BrowserJSAlert(const char*) override { s->JSDialogResponse(1); }
    void BrowserJSConfirm(const char*) override { s->JSDialogResponse(1); }
    void BrowserPopupHTMLWindow(const char* u, int, int, int, int) override { std::printf("popup: %s\n", u); }
    void BrowserSetHTMLTitle(const char* t) override { title = t; }
    void BrowserLoadingResource() override {}
    void BrowserStatusText(const char*) override {}
    void BrowserSetCursor(int) override {}
    void BrowserFileLoadDialog() override {}
    void BrowserShowToolTip(const char*) override {}
    void BrowserUpdateToolTip(const char*) override {}
    void BrowserHideToolTip() override {}
    void BrowserClose() override {}
    void BrowserLinkAtPositionResponse(const char*, int, int) override {}
};

void save_bmp(const char* path, const std::vector<unsigned char>& bgra, int w, int h) {
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = -h;
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + static_cast<DWORD>(bgra.size());
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<char*>(&fh), sizeof(fh));
    f.write(reinterpret_cast<char*>(&ih), sizeof(ih));
    f.write(reinterpret_cast<const char*>(bgra.data()), bgra.size());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::printf("usage: chromehtml_harness <dir> <url> <out.bmp> [w h] [settle_ms]\n");
        return 2;
    }
    int width = argc > 5 ? atoi(argv[4]) : 1280, height = argc > 5 ? atoi(argv[5]) : 720;
    int settle = argc > 6 ? atoi(argv[6]) : 2000;
    std::string dll = std::string(argv[1]) + "\\chromehtml.dll";
    HMODULE m = LoadLibraryA(dll.c_str());
    auto create = m ? reinterpret_cast<CreateInterfaceFn>(GetProcAddress(m, "CreateInterface")) : nullptr;
    auto* ctl = create ? static_cast<IHTMLChromeController*>(create(CHROMEHTML_CONTROLLER_INTERFACE_VERSION, nullptr))
                       : nullptr;
    if (!ctl) {
        std::printf("no %s in %s\n", CHROMEHTML_CONTROLLER_INTERFACE_VERSION, dll.c_str());
        return 1;
    }
    if (!ctl->Init("", "")) {
        std::printf("Init failed (see browser\\browser.log)\n");
        return 1;
    }
    Panel p;
    p.s = ctl->CreateSerializer(&p);
    p.s->AddHeader("Accept-Language", "ru-RU");
    p.s->BrowserSize(width, height);
    ctl->CreateBrowser(p.s->GetResponseTarget(), false, "Main Menu Browser");
    bool posted = false;
    using clock = std::chrono::steady_clock;
    auto start = clock::now(), done = clock::time_point{};
    while (clock::now() - start < std::chrono::seconds(45)) {
        ctl->RunFrame();
        if (p.ready && !posted) {  // like vgui2::HTML: PostURL once BrowserReady arrived
            p.s->PostURL(argv[2], nullptr);
            posted = true;
        }
        if (p.finished && done == clock::time_point{}) done = clock::now();
        if (done != clock::time_point{} && clock::now() - done > std::chrono::milliseconds(settle)) break;
        Sleep(10);
    }
    std::printf("ready=%d finished=%d paints=%d size=%dx%d\nurl=%s\ntitle=%s\n", p.ready, p.finished, p.paints, p.w,
                p.h, p.url.c_str(), p.title.c_str());
    if (!p.frame.empty()) save_bmp(argv[3], p.frame, p.w, p.h);
    ctl->RemoveBrowser(p.s->GetResponseTarget());
    ctl->Shutdown();
    return p.finished && !p.frame.empty() ? 0 : 1;
}
