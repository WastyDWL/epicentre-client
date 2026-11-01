// ncl://menu/<path> — the client's built-in main menu, served from <game>\platform\epicentre\menu.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nc {

constexpr char kMenuUrl[] = "ncl://menu/index.html";

// Path inside the menu folder ("index.html" for the root), percent-decoded and with / separators;
// nullopt for other hosts and anything that could leave the folder (.., drive letters, UNC).
std::optional<std::string> menu_relative_path(std::string_view url);
const char* menu_mime_type(std::string_view path);

}  // namespace nc
