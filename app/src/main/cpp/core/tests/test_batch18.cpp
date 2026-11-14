// Tests for batch 18 (DownloadNode card layout / animate, behavioural part). The bit-exact comparison against the real Java is
// tests/test_golden5.cpp; this file covers what the golden driver cannot: API contracts of the C++ seams.
#include <cstdio>
#include <memory>
#include <stdexcept>
#include "opsu/downloads/DownloadList.h"
#include "opsu/downloads/DownloadNode.h"
#include "opsu/logging/Log.h"
#include "opsu/logging/StubLogger.h"

using namespace opsu;
using DN = DownloadNode;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

struct Fonts : CardFonts {
    int lineHeight(Font) const override { return 10; }
    int width(Font, const std::string& s) const override { return static_cast<int>(s.size()) * 6; }
};

struct Src : DownloadSource {
    std::optional<std::string> getDownloadURL(int) override { return std::string("http://h/x"); }
    std::map<std::string, std::string> getDownloadRequestHeaders() override { return {}; }
    bool disableSSLInDownloads() override { return false; }
};

int main() {
    Log::setLogger(&StubLogger::get());

    // btnH / btnY / getCardRect(2) need init(): a clear error instead of Java's NullPointerException
    {
        bool threw = false;
        try { DN::btnH(); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw);
    }

    Fonts fonts;
    DN::init(1000, 800, fonts);
    CHECK(DN::btnH() == 10 * 1.7f);
    CHECK(DN::getCardRect(2, 1)[3] == DN::btnH());
    CHECK(DN::maxResultsShown() >= 1);
    CHECK(DN::COLUMNS == 3 && DN::THUMB_ASPECT == 0.9f);
    // a later init() with other fonts replaces the layout (the statics are global, like Java)
    struct Big : CardFonts {
        int lineHeight(Font) const override { return 40; }
        int width(Font, const std::string&) const override { return 0; }
    } big;
    float oldH = DN::getCardHeight();
    DN::init(1000, 800, big);
    CHECK(DN::getCardHeight() > oldH && DN::btnH() == 40 * 1.7f);
    DN::init(1000, 800, fonts);

    // fit: nothing to shorten, and a width nothing fits in still keeps one character plus "..."
    CHECK(DN::fit(fonts, CardFonts::Font::DEFAULT, "abc", 18.f) == "abc");
    CHECK(DN::fit(fonts, CardFonts::Font::DEFAULT, "abcdef", 0.f) == "a...");
    // width is 6 per byte: "abcdef..." (54), "abcde..." (48), "abcd..." (42), "abc..." (36) are too wide, "ab..." (30) fits
    CHECK(DN::fit(fonts, CardFonts::Font::DEFAULT, "abcdef", 30.f) == "ab...");

    // animate() with the global DownloadList: a created (not started) download is WAITING at 0 %
    {
        auto node = std::make_shared<DownloadNode>(4242, "d", "t", "", "a", "", "c");
        Src src;
        node->createDownload(src, "/tmp/opsu-batch18-import", false, nullptr, nullptr);
        DownloadList::get().addNode(node);
        node->animate(1.f, 0.f, 0.f, false);
        CHECK(node->isDownloadActive());
        CHECK(node->getDownloadStatus() && *node->getDownloadStatus() == Download::Status::WAITING);
        CHECK(node->getShownProgress() == 0.f && node->getProgressLabel() == "0%");
        CHECK(!node->isLoadedInLibrary());
        node->animate(1.f, 0.f, 0.f, true);
        CHECK(node->isLoadedInLibrary());

        // once the node leaves the list the lookup finds nothing: inactive, progress reset, stale status kept (Java quirk)
        DownloadList::get().remove(node.get());
        node->animate(1.f, 0.f, 0.f, false);
        CHECK(!node->isDownloadActive() && node->getShownProgress() == 0.f);
        CHECK(node->getDownloadStatus().has_value());
        node->clearDownload();
    }

    // explicit snapshot: 100 % when COMPLETE regardless of the reported progress, negative progress is clamped to 0
    {
        DownloadNode n(1, "d", "t", "", "a", "", "c");
        n.animate(1.f, 0.f, 0.f, false, DownloadSnapshot{Download::Status::COMPLETE, 12.f});
        CHECK(n.getShownProgress() == 100.f && n.getProgressLabel() == "100%");
        DownloadNode m(2, "d", "t", "", "a", "", "c");
        m.animate(1.f, 0.f, 0.f, false, DownloadSnapshot{Download::Status::DOWNLOADING, -5.f});
        CHECK(m.getShownProgress() == 0.f && m.getProgressLabel() == "0%");
    }

    Log::setLogger(nullptr);
    if (failures == 0) std::printf("batch18 OK\n");
    return failures == 0 ? 0 : 1;
}
