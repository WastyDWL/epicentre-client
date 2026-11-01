// chromehtml.dll: loaded by hw.dll, which asks it for ChromeHTML_Controller_001.
#include <windows.h>

#include <cstring>

#include "core/controller.h"

extern "C" __declspec(dllexport) void* CreateInterface(const char* name, int* return_code) {
    static nc::NcController controller;
    bool ours = name && std::strcmp(name, CHROMEHTML_CONTROLLER_INTERFACE_VERSION) == 0;
    if (return_code) *return_code = ours ? 0 : 1;  // IFACE_OK / IFACE_FAILED
    return ours ? static_cast<IHTMLChromeController*>(&controller) : nullptr;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
