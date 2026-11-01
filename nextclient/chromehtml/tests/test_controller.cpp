#include "core/controller.h"
#include "testing.h"

using namespace nc;

namespace {

struct NullPanel final : IHTMLResponses_HL1 {
    int ready = 0;
    void BrowserReady() override { ++ready; }
    void BrowserNeedsPaint(int, int, int, const unsigned char*, int, int, int, int, int, int,
                           const unsigned char*) override {}
    void BrowserStartRequest(const char*, const char*, const char*, bool) override {}
    void BrowserURLChanged(const char*, const char*, bool) override {}
    void BrowserFinishedRequest(const char*, const char*) override {}
    void BrowserShowPopup() override {}
    void BrowserHidePopup() override {}
    void BrowserSizePopup(int, int, int, int) override {}
    void BrowserHorizontalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserVerticalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserGetZoomResponse(float) override {}
    void BrowserCanGoBackandForward(bool, bool) override {}
    void BrowserJSAlert(const char*) override {}
    void BrowserJSConfirm(const char*) override {}
    void BrowserPopupHTMLWindow(const char*, int, int, int, int) override {}
    void BrowserSetHTMLTitle(const char*) override {}
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

}  // namespace

TEST(controller_serializer_has_unique_response_target) {
    NcController c;
    NullPanel a, b;
    IHTMLSerializer* sa = c.CreateSerializer(&a);
    IHTMLSerializer* sb = c.CreateSerializer(&b);
    CHECK(sa != nullptr && sb != nullptr);
    CHECK(sa->GetResponseTarget() != nullptr);
    CHECK(sa->GetResponseTarget() != sb->GetResponseTarget());
    CHECK(serializer_from_target(sa->GetResponseTarget()) == sa);
    CHECK(static_cast<NcSerializer*>(sa)->target() == &a);
}

TEST(controller_no_browser_until_created) {
    NcController c;
    NullPanel p;
    IHTMLSerializer* s = c.CreateSerializer(&p);
    CHECK_EQ(s->BrowserGetIndex(), -1);
    s->PostURL("https://epicentre-cs.ru/", nullptr);  // CEF is not running in tests: must be a no-op
    s->BrowserSize(720, 360);
    s->MouseMove(10, 10);
    CHECK_EQ(p.ready, 0);
}

TEST(controller_null_target_rejected) {
    NcController c;
    CHECK(c.CreateSerializer(nullptr) == nullptr);
    CHECK(serializer_from_target(nullptr) == nullptr);
}

TEST(controller_frame_without_engine_is_safe) {
    NcController c;
    CHECK(c.RunFrame());
    CHECK(!c.BHasPendingMessages());
    CHECK(c.GetFreeCommandBuffer(static_cast<EHTMLCommands>(0), 0) == nullptr);
}
