#pragma once

#include <string>

namespace nc {

// Serves ncl://menu/<path> from menu_root (<game>\platform\epicentre\menu). After CefInitialize.
void register_menu_scheme_handler(const std::wstring& menu_root);

}  // namespace nc
