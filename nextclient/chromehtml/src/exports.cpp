// chromehtml.dll: loaded by hw.dll, which asks it for ChromeHTML_Controller_001.
#include <windows.h>

#include <cstring>

#include "chromehtml/ncl_browser_api.h"
#include "core/controller.h"
#include "core/engine.h"

extern "C" __declspec(dllexport) void* CreateInterface(const char* name, int* return_code) {
    static nc::NcController controller;
    void* iface = nullptr;
    if (name && std::strcmp(name, CHROMEHTML_CONTROLLER_INTERFACE_VERSION) == 0)
        iface = static_cast<IHTMLChromeController*>(&controller);
    else if (name && std::strcmp(name, NCL_BROWSER_API_VERSION) == 0)
        iface = &nc::Engine::get().browser_api();
    if (return_code) *return_code = iface ? 0 : 1;  // IFACE_OK / IFACE_FAILED
    return iface;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
