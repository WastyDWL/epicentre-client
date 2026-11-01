#include "core/menu_files.h"

#include <algorithm>
#include <cctype>

namespace nc {

namespace {

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

}  // namespace

std::optional<std::string> menu_relative_path(std::string_view url) {
    constexpr std::string_view prefix = "ncl://menu";
    if (url.substr(0, prefix.size()) != prefix) return std::nullopt;
    url.remove_prefix(prefix.size());
    if (!url.empty() && url[0] != '/') return std::nullopt;  // ncl://menu.evil/
    if (!url.empty()) url.remove_prefix(1);
    url = url.substr(0, url.find_first_of("?#"));

    std::string path;
    for (size_t i = 0; i < url.size(); ++i) {
        if (url[i] == '%' && i + 2 < url.size() + 0 && hex(url[i + 1]) >= 0 && hex(url[i + 2]) >= 0) {
            path += static_cast<char>(hex(url[i + 1]) * 16 + hex(url[i + 2]));
            i += 2;
        } else {
            path += url[i];
        }
    }
    if (path.empty()) return std::string("index.html");
    // Checked after decoding: no backslashes, drives, absolute or parent paths.
    if (path.find('\\') != std::string::npos || path.find(':') != std::string::npos || path[0] == '/')
        return std::nullopt;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        std::string_view seg = std::string_view(path).substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (seg == ".." || seg == "." || seg.empty() && end != std::string::npos) return std::nullopt;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return path;
}

const char* menu_mime_type(std::string_view path) {
    std::string ext(path.substr(path.find_last_of('.') == std::string_view::npos ? path.size() : path.find_last_of('.')));
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js" || ext == ".mjs") return "text/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".woff") return "font/woff";
    if (ext == ".ttf") return "font/ttf";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".ogg") return "audio/ogg";
    if (ext == ".webm") return "video/webm";
    return "application/octet-stream";
}

}  // namespace nc
