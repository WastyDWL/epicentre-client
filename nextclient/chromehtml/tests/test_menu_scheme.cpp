#include "core/menu_files.h"
#include "testing.h"

using namespace nc;

TEST(menu_path_index_default) {
    CHECK_EQ(menu_relative_path("ncl://menu/").value_or(""), std::string("index.html"));
    CHECK_EQ(menu_relative_path("ncl://menu").value_or(""), std::string("index.html"));
}

TEST(menu_path_plain_files) {
    CHECK_EQ(menu_relative_path("ncl://menu/menu.css?v=2").value_or(""), std::string("menu.css"));
    CHECK_EQ(menu_relative_path("ncl://menu/img/map%20one.png#x").value_or(""), std::string("img/map one.png"));
}

TEST(menu_path_rejects_traversal_and_other_hosts) {
    CHECK(!menu_relative_path("ncl://menu/../../../config.cfg").has_value());
    CHECK(!menu_relative_path("ncl://menu/img/..%2F..%2Fsecret").has_value());
    CHECK(!menu_relative_path("ncl://menu/C:/Windows/win.ini").has_value());
    CHECK(!menu_relative_path(R"(ncl://menu/\\server\share)").has_value());
    CHECK(!menu_relative_path(R"(ncl://menu/img\..\..\secret)").has_value());
    CHECK(!menu_relative_path("ncl://other/index.html").has_value());
    CHECK(!menu_relative_path("https://menu/index.html").has_value());
}

TEST(menu_mime_types) {
    CHECK_EQ(std::string(menu_mime_type("index.html")), std::string("text/html"));
    CHECK_EQ(std::string(menu_mime_type("a/b.css")), std::string("text/css"));
    CHECK_EQ(std::string(menu_mime_type("x.js")), std::string("text/javascript"));
    CHECK_EQ(std::string(menu_mime_type("logo.PNG")), std::string("image/png"));
    CHECK_EQ(std::string(menu_mime_type("font.woff2")), std::string("font/woff2"));
    CHECK_EQ(std::string(menu_mime_type("data.bin")), std::string("application/octet-stream"));
}
