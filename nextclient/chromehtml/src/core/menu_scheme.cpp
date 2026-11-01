#include "core/menu_scheme.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

#include "common/ncb_scheme.h"
#include "core/menu_files.h"
#include "include/cef_scheme.h"

namespace nc {

namespace {

// One menu file (or a 404), read up front; the handler owns the bytes.
class FileHandler final : public CefResourceHandler {
public:
    FileHandler(int status, std::string mime, std::string body)
        : status_(status), mime_(std::move(mime)), body_(std::move(body)) {}

    bool Open(CefRefPtr<CefRequest>, bool& handle_request, CefRefPtr<CefCallback>) override {
        handle_request = true;
        return true;
    }
    void GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& length, CefString&) override {
        response->SetStatus(status_);
        response->SetMimeType(mime_);
        CefResponse::HeaderMap headers;
        headers.emplace("Cache-Control", "no-cache");
        response->SetHeaderMap(headers);
        length = static_cast<int64_t>(body_.size());
    }
    bool Read(void* out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback>) override {
        size_t n = std::min(static_cast<size_t>(bytes_to_read), body_.size() - offset_);
        std::memcpy(out, body_.data() + offset_, n);
        offset_ += n;
        bytes_read = static_cast<int>(n);
        return n > 0;
    }
    void Cancel() override {}

private:
    const int status_;
    const std::string mime_, body_;
    size_t offset_ = 0;

    IMPLEMENT_REFCOUNTING(FileHandler);
};

class MenuFactory final : public CefSchemeHandlerFactory {
public:
    explicit MenuFactory(std::wstring root) : root_(std::move(root)) {}

    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString&,
                                         CefRefPtr<CefRequest> request) override {
        std::optional<std::string> rel = menu_relative_path(request->GetURL().ToString());
        if (!rel) return new FileHandler(404, "text/plain", "");
        std::wstring path = root_ + L"\\" + CefString(*rel).ToWString();
        std::replace(path.begin(), path.end(), L'/', L'\\');
        std::ifstream f(path, std::ios::binary);
        if (!f) return new FileHandler(404, "text/plain", "");
        std::stringstream ss;
        ss << f.rdbuf();
        return new FileHandler(200, menu_mime_type(*rel), ss.str());
    }

private:
    const std::wstring root_;

    IMPLEMENT_REFCOUNTING(MenuFactory);
};

}  // namespace

void register_menu_scheme_handler(const std::wstring& menu_root) {
    CefRegisterSchemeHandlerFactory(kNclScheme, kNclMenuHost, new MenuFactory(menu_root));
}

}  // namespace nc
