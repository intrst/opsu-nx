// T11 tests: GameImage (the skin image registry) against a fake image device and temporary skin folders.
// Build (from core/), one line: g++ -std=c++17 -ffp-contract=off -Wall -Wextra -fsanitize=address,undefined -Iinclude
//   tests/test_gameimage_t11.cpp src/GameImage.cpp src/GameImageTable.cpp src/gfx/*.cpp src/logging/*.cpp src/util/*.cpp
// There is no JVM here, so no golden file: the expectations are derived by hand from GameImage.java (comments say how).
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include "opsu/GameImage.h"
#include "opsu/gfx/Image.h"
#include "opsu/gfx/IRenderBackend.h"
#include "opsu/logging/ILogger.h"
#include "opsu/logging/Log.h"

using namespace opsu;
namespace fs = std::filesystem;
namespace gi = opsu::gameimage;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

// ---- fakes (copied from the renderer's test, trimmed) ------------------------------------------------------------------
struct FakeTex : ITexture {
    int w, h; std::vector<uint8_t> px;
    FakeTex(int w_, int h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_ * 4, 0) {}
    int width() const override { return w; }
    int height() const override { return h; }
    void setFilter(TextureFilter, TextureFilter) override {}
    void update(int x, int y, int ww, int hh, const uint8_t* rgba) override {
        for (int r = 0; r < hh; r++) for (int c = 0; c < ww * 4; c++) px[(static_cast<size_t>(y + r) * w + x) * 4 + c] = rgba[static_cast<size_t>(r) * ww * 4 + c];
    }
    bool rowZeroIsBottom() const override { return false; }
};
struct FakeRT : IRenderTarget {
    std::shared_ptr<FakeTex> t; int w, h;
    FakeRT(int w_, int h_) : t(std::make_shared<FakeTex>(w_, h_)), w(w_), h(h_) {}
    int width() const override { return w; }
    int height() const override { return h; }
    std::shared_ptr<ITexture> texture() override { return t; }
};
struct FakeBackend : IRenderBackend {
    void drawTriangles(ITexture*, const Vertex*, int) override {}
    void setDrawMode(const DrawModeState&) override {}
    void setScissor(bool, const IRect&) override {}
    void setTarget(IRenderTarget*, const ViewportMap&) override {}
    void clear(const Color&) override {}
    void flush() override {}
};
struct FakeDecoder : IImageDecoder {
    std::map<std::string, Pixmap> files; int decodes = 0;
    bool decode(const std::string& p, Pixmap& o) override { auto it = files.find(p); if (it == files.end()) return false; decodes++; o = it->second; return true; }
};
struct FakeDevice : IRenderDevice {
    FakeBackend be; FakeDecoder dec;
    IRenderBackend& backend() override { return be; }
    std::shared_ptr<ITexture> createTexture(int w, int h, const uint8_t* rgba, TextureFilter, TextureRestorer) override {
        auto t = std::make_shared<FakeTex>(w, h);
        if (rgba) t->px.assign(rgba, rgba + static_cast<size_t>(w) * h * 4);
        return t;
    }
    std::shared_ptr<IRenderTarget> createRenderTarget(int w, int h) override { return std::make_shared<FakeRT>(w, h); }
    IImageDecoder& imageDecoder() override { return dec; }
    std::shared_ptr<IFontFace> loadFontFace(const std::string&, float) override { return nullptr; }
    void screenSize(int& w, int& h) const override { w = 1280; h = 720; }
};

/** Records what the code logs, so "the Java reports an error and carries on" can be checked. */
struct RecLogger : ILogger {
    int errors = 0, warns = 0; std::string lastError;
    void setSaveToFile(bool) override {}
    void trace(const std::string&, const std::string&) override {}
    void trace(const std::string&, const std::string&, const std::exception&) override {}
    void debug(const std::string&, const std::string&) override {}
    void debug(const std::string&, const std::string&, const std::exception&) override {}
    void info(const std::string&, const std::string&) override {}
    void info(const std::string&, const std::string&, const std::exception&) override {}
    void warn(const std::string&, const std::string&) override { warns++; }
    void warn(const std::string&, const std::string&, const std::exception&) override { warns++; }
    void error(const std::string&, const std::string& m) override { errors++; lastError = m; }
    void error(const std::string&, const std::string& m, const std::exception&) override { errors++; lastError = m; }
};

/** A game skin: folder + skin.ini values. */
struct TestSkin : SkinInfo {
    std::string dir; int fps = -1; std::string hc = "default", sc = "score", cc = "score";
    std::string getDirectory() const override { return dir; }
    int getAnimationFramerate() const override { return fps; }
    std::string getHitCircleFontPrefix() const override { return hc; }
    std::string getScoreFontPrefix() const override { return sc; }
    std::string getComboFontPrefix() const override { return cc; }
};

// ---- world ---------------------------------------------------------------------------------------------------------------
static std::shared_ptr<FakeDevice> dev;
static std::set<std::string> resNames;  // the bundled res/ files (bare names)
static fs::path root;                   // temp folder for skin directories
static RecLogger logger;
static bool hdOption = false, beatmapSkinIgnored = false;
static float mobileScale = 0.5f;

static Pixmap solid(int w, int h) {
    Pixmap p(w, h);
    for (size_t i = 0; i < p.rgba.size(); i += 4) { p.rgba[i] = 200; p.rgba[i + 1] = 100; p.rgba[i + 2] = 50; p.rgba[i + 3] = 255; }
    return p;
}
static void addRes(const std::string& bare, int w, int h) { resNames.insert(bare); dev->dec.files["res/" + bare] = solid(w, h); }
/** Creates an (empty) file in a skin folder and tells the fake decoder what size it decodes to. */
static void addFile(const fs::path& dir, const std::string& rel, int w, int h) {
    fs::path p = dir / rel;
    fs::create_directories(p.parent_path());
    std::ofstream(p).put('x');
    dev->dec.files[fs::absolute(p).string()] = solid(w, h);
}
static fs::path newDir(const std::string& name) {
    fs::path d = root / name;
    fs::remove_all(d);
    fs::create_directories(d);
    return d;
}
static std::shared_ptr<TestSkin> skinWith(const fs::path& dir) { auto s = std::make_shared<TestSkin>(); s->dir = dir.string(); return s; }

/** Fresh state for every test: no images, default skin, 1024x768 (UI scale 1), no files. */
static void reset(int w = 1024, int h = 768) {
    gi::clearReferences();
    ImageManager::instance().clearAll();
    dev->dec.files.clear();
    resNames.clear();
    hdOption = false; beatmapSkinIgnored = false; mobileScale = 0.5f;
    logger.errors = logger.warns = 0; logger.lastError.clear();
    GameImageHost h2;
    h2.resourceExists = [](const std::string& n) { return resNames.count(n) > 0; };
    h2.loadHDImages = [] { return hdOption; };
    h2.isBeatmapSkinIgnored = [] { return beatmapSkinIgnored; };
    h2.getMobileUIScale = [](float) { return mobileScale; };
    h2.fontLargeLineHeight = [] { return 40; };
    h2.fontDefaultLineHeight = [] { return 20; };
    gi::setHost(h2);
    gi::setSkinInfo(std::make_shared<SkinInfo>());
    gi::init(w, h);
}

static int W(GameImage g) { auto i = gi::getImage(g); return i ? i->getWidth() : -1; }
static int H(GameImage g) { auto i = gi::getImage(g); return i ? i->getHeight() : -1; }

// ---- 1. the generated table ------------------------------------------------------------------------------------------------
struct Row { GameImage g; const char* name; const char* file; const char* fmt; unsigned char type; bool skinnable, preload, bg, opt, scales, allows; ProcKind k; float p1, p2; };

static void testTable() {
    using G = GameImage;
    // 199 entries: counted from GameImage.java independently of the generator (lines "\tNAME (" between the enum start and ';').
    CHECK(gi::count() == 199);
    CHECK(gi::values().size() == 199);
    CHECK(static_cast<int>(G::CURSOR) == 0 && static_cast<int>(G::ALPHA_MAP) == 198);  // first and last constant of the Java enum
    CHECK(std::string(gi::name(G::CURSOR)) == "CURSOR" && std::string(gi::name(G::ALPHA_MAP)) == "ALPHA_MAP");
    // enum order: the Java declares these in this order
    CHECK(static_cast<int>(G::CURSOR) < static_cast<int>(G::CURSOR_MIDDLE) && static_cast<int>(G::CURSOR_MIDDLE) < static_cast<int>(G::CURSOR_TRAIL));
    CHECK(static_cast<int>(G::SCOREBAR_BG) < static_cast<int>(G::HIT_MISS) && static_cast<int>(G::HIT_300G) < static_cast<int>(G::DEFAULT_0));
    // every name round-trips through valueOf, and names are unique
    std::set<std::string> seen;
    for (G g : gi::values()) {
        G back = G::CURSOR; CHECK(gi::valueOf(gi::name(g), back) && back == g);
        CHECK(seen.insert(gi::name(g)).second);
        CHECK(gi::info(g).filename[0] != 0 && gi::info(g).type != 0);
    }
    G dummy; CHECK(!gi::valueOf("NOT_AN_IMAGE", dummy));

    // A sample read BY HAND from GameImage.java (constructor arguments -> flags; defaults of the short constructors are
    // skinnable = true, preload = true, bgLocked = false, optional = false):
    const unsigned char P = IMG_PNG, PJ = IMG_PNG | IMG_JPG;
    const Row rows[] = {
        // CURSOR ("cursor", "png")
        {G::CURSOR, "CURSOR", "cursor", "", P, true, true, false, false, true, true, ProcKind::None, 0, 0},
        // SECTION_PASS ("section-pass", "png") { scalesWithUI() -> false }
        {G::SECTION_PASS, "SECTION_PASS", "section-pass", "", P, true, true, false, false, false, true, ProcKind::None, 0, 0},
        // ARROW_WARNING ("arrow-warning", "png", true, true, true): the 5-argument (S,S,B,B,B) form = optional
        {G::ARROW_WARNING, "ARROW_WARNING", "arrow-warning", "", P, true, true, false, true, true, true, ProcKind::None, 0, 0},
        // SKIP ("play-skip", "play-skip-%d", "png") { scalesWithUI() -> false }
        {G::SKIP, "SKIP", "play-skip", "play-skip-%d", P, true, true, false, false, false, true, ProcKind::None, 0, 0},
        // COUNTDOWN_READY ("ready", "png") { img.getScaledCopy((h / 3f) / img.getHeight()) }
        {G::COUNTDOWN_READY, "COUNTDOWN_READY", "ready", "", P, true, true, false, false, true, true, ProcKind::FitHeightDiv, 3.f, 0},
        // COUNTDOWN_2 delegates to COUNTDOWN_READY.process_sub
        {G::COUNTDOWN_2, "COUNTDOWN_2", "count2", "", P, true, true, false, false, true, true, ProcKind::FitHeightDiv, 3.f, 0},
        // PAUSE_OVERLAY ("pause-overlay", "png|jpg") { img.getScaledCopy((float) w / img.getWidth()) }
        {G::PAUSE_OVERLAY, "PAUSE_OVERLAY", "pause-overlay", "", PJ, true, true, false, false, true, true, ProcKind::FitWidth, 0, 0},
        {G::FAIL_BACKGROUND, "FAIL_BACKGROUND", "fail-background", "", PJ, true, true, false, false, true, true, ProcKind::FitWidth, 0, 0},
        // SLIDER_STARTCIRCLE ("sliderstartcircle", "png", true, true, true)
        {G::SLIDER_STARTCIRCLE, "SLIDER_STARTCIRCLE", "sliderstartcircle", "", P, true, true, false, true, true, true, ProcKind::None, 0, 0},
        // SLIDER_BALL ("sliderb", "sliderb%d", "png")
        {G::SLIDER_BALL, "SLIDER_BALL", "sliderb", "sliderb%d", P, true, true, false, false, true, true, ProcKind::None, 0, 0},
        // FOLLOWPOINT ("followpoint", "followpoint-%d", "png")
        {G::FOLLOWPOINT, "FOLLOWPOINT", "followpoint", "followpoint-%d", P, true, true, false, false, true, true, ProcKind::None, 0, 0},
        // SPINNER_BACKGROUND ("spinner-background", "png", true, true, true)
        {G::SPINNER_BACKGROUND, "SPINNER_BACKGROUND", "spinner-background", "", P, true, true, false, true, true, true, ProcKind::None, 0, 0},
        // SCOREBAR_BG ("scorebar-bg", "png"): not locked itself
        {G::SCOREBAR_BG, "SCOREBAR_BG", "scorebar-bg", "", P, true, true, false, false, true, true, ProcKind::None, 0, 0},
        // SCOREBAR_COLOUR ("scorebar-colour", "scorebar-colour-%d", "png", true): (S,S,S,B) = format + bgLocked
        {G::SCOREBAR_COLOUR, "SCOREBAR_COLOUR", "scorebar-colour", "scorebar-colour-%d", P, true, true, true, false, true, true, ProcKind::None, 0, 0},
        // SCOREBAR_MARKER ("scorebar-marker", "png", true): (S,S,B) = bgLocked
        {G::SCOREBAR_MARKER, "SCOREBAR_MARKER", "scorebar-marker", "", P, true, true, true, false, true, true, ProcKind::None, 0, 0},
        // HIT_300 ("hit300", "hit300-%d", "png")
        {G::HIT_300, "HIT_300", "hit300", "hit300-%d", P, true, true, false, false, true, true, ProcKind::None, 0, 0},
        // DEFAULT_0 ("default-0", "png")
        {G::DEFAULT_0, "DEFAULT_0", "default-0", "", P, true, true, false, false, true, true, ProcKind::None, 0, 0},
        // SELECTION_MODS ("selection-mods", "png", false, false) { scaleMin1px(img, Options.getMobileUIScale(0.5f)) }
        {G::SELECTION_MODS, "SELECTION_MODS", "selection-mods", "", P, false, false, false, false, true, true, ProcKind::ScaleMin1pxMobileUI, 0.5f, 0},
        // VOLUME ("volume-bg", "png", false, false) { img.getScaledCopy((h * 0.3f) / img.getHeight()) }
        {G::VOLUME, "VOLUME", "volume-bg", "", P, false, false, false, false, true, true, ProcKind::FitHeightMul, 0.3f, 0},
        // MENU_BACK ("menu-back", "menu-back-%d", "png", false, true): no override (the next entry, MENU_BACK_CHEVRON, follows at once)
        {G::MENU_BACK, "MENU_BACK", "menu-back", "menu-back-%d", P, false, true, false, false, true, true, ProcKind::None, 0, 0},
        // MENU_BUTTON_BG ("menu-button-background", "png", false, false) { img.getScaledCopy(Options.getMobileUIScale(0.5f)) }
        {G::MENU_BUTTON_BG, "MENU_BUTTON_BG", "menu-button-background", "", P, false, false, false, false, true, true, ProcKind::ScaleMobileUI, 0.5f, 0},
        // STAR ("star", "png", false, false) { (MENU_BUTTON_BG.getImage().getHeight() * 0.16f) / img.getHeight() }
        {G::STAR, "STAR", "star", "", P, false, false, false, false, true, true, ProcKind::FitButtonBgHeight, 0.16f, 0},
        // USER ("user", "user%d", "png", false, false)
        {G::USER, "USER", "user", "user%d", P, false, false, false, false, true, true, ProcKind::None, 0, 0},
        // MENU_TAB ("selection-tab", "png", false, false) { process_sub = tab size; allowsSkin() -> false }
        {G::MENU_TAB, "MENU_TAB", "selection-tab", "", P, false, false, false, false, true, false, ProcKind::MenuTab, 0, 0},
        // MENU_BG ("menu-background", "png|jpg", false, true) { alpha 0.9 + cover fit }
        {G::MENU_BG, "MENU_BG", "menu-background", "", PJ, false, true, false, false, true, true, ProcKind::MenuBgCover, 0, 0},
        // SEARCH_BG1 ("appBG1", "png|jpg", false, true, true) { img.setAlpha(0.8f); img.getScaledCopy(w, h) }
        {G::SEARCH_BG1, "SEARCH_BG1", "appBG1", "", PJ, false, true, false, true, true, true, ProcKind::StretchWHAlpha, 0.8f, 0},
        // SELECTION_MODE ("selection-mode", "png", false, false, true) { scalesWithUI() -> false }
        {G::SELECTION_MODE, "SELECTION_MODE", "selection-mode", "", P, false, false, false, true, false, true, ProcKind::None, 0, 0},
        // ALPHA_MAP ("alphaInvert", "png", false, false)
        {G::ALPHA_MAP, "ALPHA_MAP", "alphaInvert", "", P, false, false, false, false, true, true, ProcKind::None, 0, 0},
    };
    for (const Row& r : rows) {
        const auto& i = gi::info(r.g);
        bool ok = std::string(i.name) == r.name && std::string(i.filename) == r.file && std::string(i.filenameFormat) == r.fmt && i.type == r.type &&
                  i.beatmapSkinnable == r.skinnable && i.preload == r.preload && i.bgLocked == r.bg && i.optional == r.opt &&
                  i.scalesWithUI == r.scales && i.allowsSkin == r.allows && i.proc == r.k && i.p1 == r.p1 && i.p2 == r.p2;
        if (!ok) std::printf("  table row differs: %s\n", r.name);
        CHECK(ok);
        CHECK(gi::isBeatmapSkinnable(r.g) == r.skinnable && gi::isPreload(r.g) == r.preload);
    }
    CHECK(sizeof(rows) / sizeof(rows[0]) >= 20);

    // consistency of the whole table: a format pattern has exactly one "%d"; only the entries with an override have a ProcKind
    int animated = 0, withKind = 0;
    for (G g : gi::values()) {
        std::string f = gi::info(g).filenameFormat;
        if (!f.empty()) { animated++; CHECK(f.find("%d") != std::string::npos && f.find("%d") == f.rfind("%d")); }
        if (gi::info(g).proc != ProcKind::None) withKind++;
    }
    CHECK(animated == 14);   // the 14 format patterns of the Java file (grep '%d' GameImage.java, 14 entries)
    CHECK(withKind == 41);   // the Java has 41 process_sub overrides (counted from the source with comments removed; the commented-out DELETE one is not counted)
}

// ---- 2. lookup order -----------------------------------------------------------------------------------------------------------
static void testLookupOrder() {
    using G = GameImage;
    const fs::path gameSkin = newDir("gameskin"), bmSkin = newDir("bmskin");
    // sizes tell where an image came from: beatmap skin 100, game skin 80, res/ 60 (UI scale 1: no scaling)
    reset();
    addRes("cursor.png", 60, 60);
    CHECK(W(G::CURSOR) == 60 && H(G::CURSOR) == 60);              // default skin: res/ only
    CHECK(!gi::hasGameSkinImage(G::CURSOR) && !gi::isProvidedByGameSkin(G::CURSOR) && !gi::hasBeatmapSkinImage(G::CURSOR));
    CHECK(gi::getRawWidth(G::CURSOR) == 60);

    // game skin wins over res/
    reset();
    addRes("cursor.png", 60, 60); addFile(gameSkin, "cursor.png", 80, 80);
    gi::setSkinInfo(skinWith(gameSkin));
    CHECK(W(G::CURSOR) == 80 && gi::hasGameSkinImage(G::CURSOR) && gi::isProvidedByGameSkin(G::CURSOR));
    CHECK(gi::getDefaultImage(G::CURSOR)->getWidth() == 80);

    // beatmap skin wins over game skin; the default image is still the game skin's
    addFile(bmSkin, "cursor.png", 100, 100);
    CHECK(gi::setBeatmapSkinImage(G::CURSOR, bmSkin.string()));
    CHECK(W(G::CURSOR) == 100 && gi::hasBeatmapSkinImage(G::CURSOR) && gi::isProvidedByBeatmapSkin(G::CURSOR));
    CHECK(gi::getDefaultImage(G::CURSOR)->getWidth() == 80 && gi::getRawWidth(G::CURSOR) == 100);
    // a beatmap skin folder that lacks the file: the Java destroys the old beatmap image first, then returns false -> game skin again
    const fs::path emptyBm = newDir("bmempty");
    auto old = gi::getImage(G::CURSOR);
    CHECK(!gi::setBeatmapSkinImage(G::CURSOR, emptyBm.string()));
    CHECK(old->isDestroyed() && !gi::hasBeatmapSkinImage(G::CURSOR) && W(G::CURSOR) == 80);
    // no folder / beatmap skins ignored: false, nothing loaded
    CHECK(!gi::setBeatmapSkinImage(G::CURSOR, ""));
    beatmapSkinIgnored = true;
    CHECK(!gi::setBeatmapSkinImage(G::CURSOR, bmSkin.string()) && W(G::CURSOR) == 80);
    beatmapSkinIgnored = false;
    // the beatmap skin never provides a beatmap-unskinnable image like MENU_TAB (allowsSkin() false)
    addFile(bmSkin, "selection-tab.png", 100, 100);
    CHECK(!gi::setBeatmapSkinImage(G::MENU_TAB, bmSkin.string()));

    // the game skin lacks the file: falls back to res/
    reset();
    addRes("cursor.png", 60, 60);
    gi::setSkinInfo(skinWith(newDir("gameskin_empty")));
    CHECK(W(G::CURSOR) == 60 && !gi::hasGameSkinImage(G::CURSOR) && !gi::isProvidedByGameSkin(G::CURSOR));

    // allowsSkin() == false (MENU_TAB): the game skin's file is ignored, res/ is used (scaled by its own rule, see test 5)
    reset();
    addRes("selection-tab.png", 222, 42); addFile(gameSkin, "selection-tab.png", 10, 10);
    gi::setSkinInfo(skinWith(gameSkin));
    CHECK(gi::getRawWidth(G::MENU_TAB) == 0);                // nothing loaded yet
    CHECK(gi::getImage(G::MENU_TAB) != nullptr && gi::getRawWidth(G::MENU_TAB) == 222 && !gi::hasGameSkinImage(G::MENU_TAB));

    // missing everywhere: a normal entry gives null and the Java's ErrorHandler.error ("Could not find default image 'hitcircle'.")
    reset();
    Log::setLogger(&logger);
    CHECK(gi::getImage(G::HITCIRCLE) == nullptr);
    CHECK(logger.errors == 1 && logger.lastError.find("Could not find default image 'hitcircle'.") != std::string::npos);
    CHECK(!gi::exists(G::HITCIRCLE));
    // an optional entry is silent and remembered as absent (no new lookup after a file appears)
    logger.errors = 0;
    CHECK(gi::getImage(G::ARROW_WARNING) == nullptr && logger.errors == 0);
    addRes("arrow-warning.png", 30, 30);
    CHECK(gi::getImage(G::ARROW_WARNING) == nullptr);        // absentInSkin sticks until clearReferences()
    gi::clearReferences();
    CHECK(gi::getImage(G::ARROW_WARNING) != nullptr);
    Log::setLogger(nullptr);

    // no skin object at all (Java Options.getSkin() == null): nothing is loaded
    reset();
    addRes("cursor.png", 60, 60);
    gi::setSkinInfo(nullptr);
    CHECK(gi::getImage(G::CURSOR) == nullptr);
    gi::setSkinInfo(std::make_shared<SkinInfo>());
    CHECK(gi::getImage(G::CURSOR) != nullptr);

    // png|jpg entries: png is tried first, then jpg (names list order in getImageFileName)
    reset();
    addRes("menu-background.jpg", 400, 300);
    CHECK(gi::getImage(G::MENU_BG) != nullptr && gi::getRawWidth(G::MENU_BG) == 400);
    reset();
    addRes("menu-background.jpg", 400, 300); addRes("menu-background.png", 500, 300);
    CHECK(gi::getImage(G::MENU_BG) != nullptr && gi::getRawWidth(G::MENU_BG) == 500);
    reset();
    addFile(gameSkin, "menu-background.jpg", 640, 480); addRes("menu-background.png", 500, 300);   // a skin jpg beats the res png
    gi::setSkinInfo(skinWith(gameSkin));
    CHECK(gi::getImage(G::MENU_BG) != nullptr && gi::getRawWidth(G::MENU_BG) == 640);

    // scorebar parts (bgLocked): come from the skin only if the skin has scorebar-bg
    reset();
    addRes("scorebar-bg.png", 50, 10); addRes("scorebar-marker.png", 9, 9);
    const fs::path sbSkin = newDir("sbskin");
    addFile(sbSkin, "scorebar-marker.png", 7, 7);            // the skin has a marker but no scorebar-bg
    gi::setSkinInfo(skinWith(sbSkin));
    CHECK(gi::getImage(G::SCOREBAR_BG) != nullptr && !gi::hasGameSkinImage(G::SCOREBAR_BG));
    CHECK(gi::getImage(G::SCOREBAR_MARKER) != nullptr && gi::getRawWidth(G::SCOREBAR_MARKER) == 9);   // res/ marker, the skin's is ignored
    reset();
    addRes("scorebar-bg.png", 50, 10); addRes("scorebar-marker.png", 9, 9);
    const fs::path sbSkin2 = newDir("sbskin2");
    addFile(sbSkin2, "scorebar-bg.png", 70, 12);             // the skin has scorebar-bg but no marker
    gi::setSkinInfo(skinWith(sbSkin2));
    CHECK(gi::getImage(G::SCOREBAR_BG) != nullptr && gi::hasGameSkinImage(G::SCOREBAR_BG));
    CHECK(gi::getImage(G::SCOREBAR_MARKER) == nullptr && !gi::exists(G::SCOREBAR_MARKER));   // absentInSkin: not drawn, no res/ fallback

    // font sprites: root file > fonts/<kind>/ folder; ComboPrefix defaults to "score" and combo-N is never looked up in the skin root
    reset();
    const fs::path fontSkin = newDir("fontskin");
    addFile(fontSkin, "default-0.png", 11, 11);              // root file
    addFile(fontSkin, "fonts/hitcircle/default-1.png", 12, 12);   // only in the fonts folder (case-insensitive folder names also work)
    const fs::path caseSkin = newDir("caseskin");            // a separate skin: a skin with both "fonts" and "Fonts" would hit the exact name first
    addFile(caseSkin, "Fonts/Score/SCORE-3.PNG", 13, 13);    // odd case in folder AND file name: only the ignore-case lookup finds it
    addRes("default-0.png", 1, 1); addRes("default-1.png", 2, 2); addRes("score-3.png", 3, 3); addRes("score-4.png", 4, 4);
    gi::setSkinInfo(skinWith(fontSkin));
    CHECK(gi::getImage(G::DEFAULT_0) != nullptr && gi::getRawWidth(G::DEFAULT_0) == 11);
    CHECK(gi::getImage(G::DEFAULT_1) != nullptr && gi::getRawWidth(G::DEFAULT_1) == 12);
    // "SCORE-3.PNG": findImageIgnoreCase looks for "score-3.png" ignoring case -> found in Fonts/Score (13), not res (3)
    gi::clearReferences();
    gi::setSkinInfo(skinWith(caseSkin));
    CHECK(gi::getImage(G::SCORE_3) != nullptr && gi::getRawWidth(G::SCORE_3) == 13);
    CHECK(gi::getImage(G::SCORE_4) != nullptr && gi::getRawWidth(G::SCORE_4) == 4);   // res/ has score-4 only: the skin lacks it
    // skin.ini prefix: HitCirclePrefix "fonts/hit/mine" -> fonts/hit/mine-0.png
    reset();
    const fs::path prefSkin = newDir("prefskin");
    addFile(prefSkin, "fonts/hit/mine-0.png", 21, 21);
    addRes("default-0.png", 1, 1);
    auto ps = skinWith(prefSkin); ps->hc = "fonts\\hit\\mine";   // backslashes are converted
    gi::setSkinInfo(ps);
    CHECK(gi::getImage(G::DEFAULT_0) != nullptr && gi::getRawWidth(G::DEFAULT_0) == 21);

    // the SkinCache seam: isListed == 0 means "known absent": the file system is not asked
    reset();
    addRes("cursor.png", 60, 60); addFile(gameSkin, "cursor.png", 80, 80);
    gi::setSkinInfo(skinWith(gameSkin));
    GameImageHost h = GameImageHost();
    h.resourceExists = [](const std::string& n) { return resNames.count(n) > 0; };
    h.isListed = [](const std::string&, const std::string& n) { return n == "cursor.png" ? 0 : -1; };
    gi::setHost(h);
    CHECK(W(G::CURSOR) == 60);
}

// ---- 3. HD suffix and UI scale ---------------------------------------------------------------------------------------------------------
static void testHDAndScale() {
    using G = GameImage;
    // HD: with loadHDImages() and uiscale >= 1, "name@2x" is tried first and halved (getScaledCopy(0.5f)); raw size = halved size
    reset();                                                // 1024x768 -> uiscale exactly 1
    hdOption = true;
    addRes("cursor@2x.png", 200, 200); addRes("cursor.png", 60, 60);
    CHECK(W(G::CURSOR) == 100 && gi::getRawWidth(G::CURSOR) == 100);   // @2x chosen: 200 * 0.5
    // HD option off: SD file
    reset(); hdOption = false;
    addRes("cursor@2x.png", 200, 200); addRes("cursor.png", 60, 60);
    CHECK(W(G::CURSOR) == 60);
    // uiscale < 1 (1024x600 -> 0.78125): HD is not used even if the option is on
    reset(1024, 600); hdOption = true;
    addRes("cursor@2x.png", 200, 200); addRes("cursor.png", 60, 60);
    CHECK(gi::getUIscale() < 1.f && gi::getRawWidth(G::CURSOR) == 0);
    CHECK(gi::getImage(G::CURSOR) != nullptr && gi::getRawWidth(G::CURSOR) == 60);
    // HD asked but only the SD file exists: falls back to it (suffix list {"@2x", ""})
    reset(); hdOption = true;
    addRes("cursor.png", 60, 60);
    CHECK(W(G::CURSOR) == 60);
    // HD in a game skin folder beats an SD file in the same folder, and the skin beats a res/ @2x
    reset(); hdOption = true;
    const fs::path hdSkin = newDir("hdskin");
    addFile(hdSkin, "cursor@2x.png", 160, 160); addFile(hdSkin, "cursor.png", 70, 70); addRes("cursor@2x.png", 400, 400);
    gi::setSkinInfo(skinWith(hdSkin));
    CHECK(W(G::CURSOR) == 80 && gi::hasGameSkinImage(G::CURSOR));
    // animation frames also get the HD suffix: "followpoint-0@2x", "followpoint-1@2x" (halved), then SD frames if no HD frame 0
    reset(); hdOption = true;
    addRes("followpoint-0@2x.png", 40, 40); addRes("followpoint-1@2x.png", 40, 40); addRes("followpoint-0.png", 10, 10);
    CHECK(gi::getImages(G::FOLLOWPOINT).size() == 2 && gi::getImages(G::FOLLOWPOINT)[0]->getWidth() == 20);
    // the exact boundary: uiscale 1.0 allows HD (>=), the float uiscale of 768/768 is exactly 1
    CHECK(gi::getUIscale() == 1.f);

    // UI scale: the loaded image is scaled by uiscale = height / 768 (scaleMin1px: (int)(w * scale), at least 1)
    reset(1536, 1536);                                      // uiscale = 2
    addRes("cursor.png", 60, 40); addRes("hitcircle.png", 1, 1);
    CHECK(gi::getUIscale() == 2.f && W(G::CURSOR) == 120 && H(G::CURSOR) == 80);
    CHECK(gi::getRawWidth(G::CURSOR) == 60 && gi::getRawHeight(G::CURSOR) == 40);   // raw = before scaling
    reset(1024, 384);                                       // uiscale = 0.5
    addRes("cursor.png", 61, 40); addRes("hitcircle.png", 1, 1);
    CHECK(W(G::CURSOR) == 30 && H(G::CURSOR) == 20);        // (int)(61 * 0.5f) = 30 (truncates)
    CHECK(W(G::HITCIRCLE) == 1 && H(G::HITCIRCLE) == 1);    // (int)(1 * 0.5f) = 0 -> max(1, 0) = 1: never collapses to 0x0
    // scalesWithUI() == false keeps the loaded pixel size (SECTION_PASS, SKIP)
    reset(1536, 1536);
    addRes("section-pass.png", 50, 30);
    CHECK(W(G::SECTION_PASS) == 50 && H(G::SECTION_PASS) == 30);
    // init() after load does not touch loaded images, but changes getUIscale() and is used by the next load
    gi::init(1024, 768);
    CHECK(gi::getUIscale() == 1.f && W(G::SECTION_PASS) == 50);
    CHECK(gi::getUIscale() == static_cast<float>(768) / 768.f);
}


// ---- 4. animation frames and frame rate ------------------------------------------------------------------------------------------------
static int64_t fakeNow = 1000;
static int64_t fakeClock() { return fakeNow; }

static void testAnimations() {
    using G = GameImage;
    const fs::path skin = newDir("animskin"), bm = newDir("animbm");
    // frame discovery: name-0, name-1, ... until the first missing number ("followpoint-3" is ignored: there is no -2)
    reset();
    addRes("followpoint-0.png", 10, 10); addRes("followpoint-1.png", 10, 10); addRes("followpoint-3.png", 10, 10);
    CHECK(gi::getImages(G::FOLLOWPOINT).size() == 2);
    CHECK(gi::getImage(G::FOLLOWPOINT) == nullptr);                 // no plain "followpoint.png": only frames (FOLLOWPOINT does not keep a plain image)
    CHECK(gi::getFrames(G::FOLLOWPOINT).size() == 2 && gi::getFirstFrame(G::FOLLOWPOINT) == gi::getImages(G::FOLLOWPOINT)[0]);
    CHECK(gi::exists(G::FOLLOWPOINT));
    // the pattern without a separator: SLIDER_BALL "sliderb%d" starts at sliderb0
    reset();
    addRes("sliderb0.png", 30, 30); addRes("sliderb1.png", 30, 30); addRes("sliderb2.png", 30, 30);
    CHECK(gi::getImages(G::SLIDER_BALL).size() == 3);
    // frames win over the single image of the same entry (the single file is only loaded for the judgement entries)
    reset();
    addRes("followpoint.png", 20, 20); addRes("followpoint-0.png", 10, 10); addRes("followpoint-1.png", 10, 10);
    CHECK(gi::getImages(G::FOLLOWPOINT).size() == 2 && gi::getImage(G::FOLLOWPOINT) == nullptr);
    // a single image only: no frames, getFrames gives the one image, getImages is empty
    reset();
    addRes("followpoint.png", 20, 20);
    CHECK(gi::getImages(G::FOLLOWPOINT).empty() && gi::getImage(G::FOLLOWPOINT) != nullptr);
    CHECK(gi::getFrames(G::FOLLOWPOINT).size() == 1 && gi::getFrames(G::FOLLOWPOINT)[0] == gi::getImage(G::FOLLOWPOINT));
    // judgements (keepsPlainImage): frames AND the plain image, and every one is scaled by the UI scale (here 2)
    reset(1536, 1536);
    addRes("hit300-0.png", 10, 10); addRes("hit300-1.png", 10, 10); addRes("hit300.png", 12, 12);
    CHECK(gi::getImages(G::HIT_300).size() == 2 && gi::getImages(G::HIT_300)[1]->getWidth() == 20);
    CHECK(gi::getImage(G::HIT_300) != nullptr && gi::getImage(G::HIT_300)->getWidth() == 24);
    CHECK(gi::getPlainImage(G::HIT_300) == gi::getImage(G::HIT_300));
    // an entry without the judgement rule (FOLLOWPOINT) keeps no plain image even if the file exists (checked above); a skin
    // that has only the plain hit300: getPlainImage is that image, getImages empty
    reset();
    addFile(skin, "hit300.png", 15, 15);
    gi::setSkinInfo(skinWith(skin));
    CHECK(gi::getImages(G::HIT_300).empty() && gi::getPlainImage(G::HIT_300)->getWidth() == 15);
    // the game skin's frames replace the res/ frames completely (the skin has the first frame, so res/ is not consulted)
    reset();
    addRes("followpoint-0.png", 10, 10); addRes("followpoint-1.png", 10, 10); addRes("followpoint-2.png", 10, 10);
    addFile(skin, "followpoint-0.png", 40, 40);
    gi::setSkinInfo(skinWith(skin));
    CHECK(gi::getImages(G::FOLLOWPOINT).size() == 1 && gi::getImages(G::FOLLOWPOINT)[0]->getWidth() == 40);
    // beatmap skin frames take priority over the game skin's, getDefaultImages keeps the game skin's
    addFile(bm, "followpoint-0.png", 50, 50); addFile(bm, "followpoint-1.png", 50, 50);
    CHECK(gi::setBeatmapSkinImage(G::FOLLOWPOINT, bm.string()));
    CHECK(gi::hasBeatmapSkinImages(G::FOLLOWPOINT) && gi::getImages(G::FOLLOWPOINT).size() == 2 && gi::getFrames(G::FOLLOWPOINT).size() == 2);
    CHECK(gi::getDefaultImages(G::FOLLOWPOINT).size() == 1);
    gi::destroyBeatmapSkinImage(G::FOLLOWPOINT);
    CHECK(!gi::hasBeatmapSkinImages(G::FOLLOWPOINT) && gi::getImages(G::FOLLOWPOINT).size() == 1);
    // setImage(img, index): replaces a frame, an index past the end (or negative) is ignored
    auto repl = std::make_shared<Image>(gi::getImages(G::FOLLOWPOINT)[0]->getScaledCopy(2.f));
    gi::setImage(G::FOLLOWPOINT, repl, 0); gi::setImage(G::FOLLOWPOINT, repl, 5); gi::setImage(G::FOLLOWPOINT, repl, -1);
    CHECK(gi::getImages(G::FOLLOWPOINT).size() == 1 && gi::getImages(G::FOLLOWPOINT)[0] == repl);

    // getAnimation(): the frame time is 1000 / framerate (integer division); Animation::updateNoDraw advances once the frame was
    // shown MORE than that many milliseconds, so the duration is observable with a fake clock.
    Animation::setClockForTests(fakeClock);
    reset();
    for (int i = 0; i < 3; i++) addRes("followpoint-" + std::to_string(i) + ".png", 10, 10);
    auto sk = std::make_shared<TestSkin>(); sk->fps = 30; gi::setSkinInfo(sk);      // 1000 / 30 = 33 ms
    fakeNow = 1000;
    Animation a = gi::getAnimation(G::FOLLOWPOINT);
    CHECK(a.getFrameCount() == 3 && a.getCurrentFrame() == gi::getImages(G::FOLLOWPOINT)[0]);   // the Animation shares the frames
    fakeNow = 1033; a.updateNoDraw(); CHECK(a.getCurrentFrame() == gi::getImages(G::FOLLOWPOINT)[0]);   // 33 > 33 is false
    fakeNow = 1034; a.updateNoDraw(); CHECK(a.getCurrentFrame() == gi::getImages(G::FOLLOWPOINT)[1]);
    // framerate -1 ("one frame per image"): fps = number of images (3) -> 1000 / 3 = 333 ms
    sk->fps = -1;
    fakeNow = 5000;
    Animation b = gi::getAnimation(G::FOLLOWPOINT);
    fakeNow = 5333; b.updateNoDraw(); CHECK(b.getCurrentFrame() == gi::getImages(G::FOLLOWPOINT)[0]);
    fakeNow = 5334; b.updateNoDraw(); CHECK(b.getCurrentFrame() == gi::getImages(G::FOLLOWPOINT)[1]);
    // explicit duration
    fakeNow = 9000;
    Animation c = gi::getAnimation(G::FOLLOWPOINT, 100);
    fakeNow = 9100; c.updateNoDraw(); CHECK(c.getCurrentFrame() == gi::getImages(G::FOLLOWPOINT)[0]);
    fakeNow = 9101; c.updateNoDraw(); CHECK(c.getCurrentFrame() == gi::getImages(G::FOLLOWPOINT)[1]);
    // no frames: the single image is a one frame animation; fps 0 is guarded (Java: division by zero)
    reset();
    addRes("followpoint.png", 20, 20);
    sk = std::make_shared<TestSkin>(); sk->fps = 0; gi::setSkinInfo(sk);
    Animation d = gi::getAnimation(G::FOLLOWPOINT);
    CHECK(d.getFrameCount() == 1 && d.getCurrentFrame() == gi::getImage(G::FOLLOWPOINT));
    // no image at all: an empty Animation and an error in the log (Java: a NullPointerException later)
    reset();
    Log::setLogger(&logger);
    Animation e = gi::getAnimation(G::FOLLOWPOINT);
    CHECK(e.getFrameCount() == 0);
    Log::setLogger(nullptr);
    Animation::setClockForTests(nullptr);
}

// ---- 5. process_sub overrides -----------------------------------------------------------------------------------------------------------------
// process() calls process_sub(img, w, h) with h = 768 and w = 768 * containerWidth / containerHeight (integer division), then
// (if scalesWithUI) scaleMin1px(img, uiscale = containerHeight / 768). Image sizes are chosen so every float step is exact.
static void testProcessOverrides() {
    using G = GameImage;
    // COUNTDOWN_READY: scale = (h / 3f) / img.getHeight() = 256 / 128 = 2 -> 100x128 becomes 200x256. 1024x768: uiscale 1.
    reset();
    addRes("ready.png", 100, 128); addRes("count2.png", 100, 128); addRes("go.png", 50, 64);
    CHECK(W(G::COUNTDOWN_READY) == 200 && H(G::COUNTDOWN_READY) == 256);
    CHECK(W(G::COUNTDOWN_2) == 200 && H(G::COUNTDOWN_2) == 256);      // delegates to COUNTDOWN_READY
    CHECK(W(G::COUNTDOWN_GO) == 200 && H(G::COUNTDOWN_GO) == 256);    // same rule, own copy of the code: 256 / 64 = 4, 50x64 -> 200x256
    CHECK(gi::getRawWidth(G::COUNTDOWN_READY) == 100 && gi::getRawHeight(G::COUNTDOWN_READY) == 128);
    // both stages: uiscale 2 (1536x1536, w = 768 * 1536 / 1536 = 768): 100x128 -> x2 (override) -> 200x256 -> x2 (UI scale) = 400x512
    reset(1536, 1536);
    addRes("ready.png", 100, 128);
    CHECK(W(G::COUNTDOWN_READY) == 400 && H(G::COUNTDOWN_READY) == 512);
    // PAUSE_OVERLAY: (float) w / img.getWidth() = 1536 / 512 = 3 on a 1536x768 screen (w = 768 * 1536 / 768 = 1536): 512x256 -> 1536x768
    reset(1536, 768);
    addRes("pause-overlay.png", 512, 256);
    CHECK(W(G::PAUSE_OVERLAY) == 1536 && H(G::PAUSE_OVERLAY) == 768);
    // SEARCH_BG: img.getScaledCopy(w, h) stretches to (w, h) = (1536, 768) whatever the aspect ratio
    addRes("search-background.png", 100, 300);
    CHECK(W(G::SEARCH_BG) == 1536 && H(G::SEARCH_BG) == 768);
    // MENU_BG cover: scale = max(w / iw, h / ih); 1024x384 on 1024x768: max(1, 2) = 2 -> 2048x768 (the overflow is cropped by the caller)
    reset();
    addRes("menu-background.png", 1024, 384);
    CHECK(W(G::MENU_BG) == 2048 && H(G::MENU_BG) == 768);
    // ... and 2048x1536: max(0.5, 0.5) = 0.5 -> 1024x768
    reset();
    addRes("menu-background.jpg", 2048, 1536);
    CHECK(W(G::MENU_BG) == 1024 && H(G::MENU_BG) == 768);
    // ScaleFixed 0.8 (MENU_LOGO): 100x50 -> 80x40
    reset();
    addRes("logo.png", 100, 50);
    CHECK(W(G::MENU_LOGO) == 80 && H(G::MENU_LOGO) == 40);
    // ScaleMobileUI: Options.getMobileUIScale(0.5f) comes from the host seam (here 0.5 and then 0.25)
    reset();
    addRes("menu-button-background.png", 200, 100);
    CHECK(W(G::MENU_BUTTON_BG) == 100 && H(G::MENU_BUTTON_BG) == 50);
    reset(); mobileScale = 0.25f;
    addRes("menu-button-background.png", 200, 100);
    CHECK(W(G::MENU_BUTTON_BG) == 50 && H(G::MENU_BUTTON_BG) == 25);
    // ScaleMin1pxMobileUI (SELECTION_MODS): 40x20 -> 10x5 and 3x3 -> (int)(0.75) = 0 -> 1x1
    addRes("selection-mods.png", 40, 20); addRes("selection-random.png", 3, 3);
    CHECK(W(G::SELECTION_MODS) == 10 && H(G::SELECTION_MODS) == 5);
    CHECK(W(G::SELECTION_RANDOM) == 1 && H(G::SELECTION_RANDOM) == 1);
    // FitButtonBgHeight (STAR) uses the processed MENU_BUTTON_BG height: 200x100 at 0.5 -> height 50; STAR 20x10, (50 * 0.16f) / 10 = 0.8 -> 16x8
    reset();
    addRes("menu-button-background.png", 200, 100); addRes("star.png", 20, 10);
    CHECK(W(G::STAR) == 16 && H(G::STAR) == 8);
    // without MENU_BUTTON_BG the Java throws a NullPointerException; here the star stays unscaled and an error is logged
    reset(); Log::setLogger(&logger);
    addRes("star.png", 20, 10);
    CHECK(W(G::STAR) == 20 && logger.errors >= 1);
    Log::setLogger(nullptr);
    // MenuTab: th = 768 * 0.033f * 0.5f = 12.672 -> round 13; tw = th * 222f / 42f = 66.98 -> 67. Any source size becomes 67x13.
    reset();
    addRes("selection-tab.png", 222, 42);
    CHECK(W(G::MENU_TAB) == 67 && H(G::MENU_TAB) == 13);
    // font based: r = (int)((Fonts.LARGE.getLineHeight() + Fonts.DEFAULT.getLineHeight() - 8) / uiscale) = (40 + 20 - 8) / 1 = 52
    reset();
    addRes("music-note.png", 10, 20); addRes("loader.png", 96, 48);
    CHECK(W(G::MENU_MUSICNOTE) == 52 && H(G::MENU_MUSICNOTE) == 52);             // getScaledCopy(r, r)
    CHECK(W(G::MENU_LOADER) == 104 && H(G::MENU_LOADER) == 52);                  // getScaledCopy(r / 48f): 96x48 * 1.0833 -> 104x52
    // the same at uiscale 2 (1536x1536): r = 52 / 2 = 26; the final UI scale doubles the size again: 52x52
    reset(1536, 1536);
    addRes("music-note.png", 10, 20);
    CHECK(W(G::MENU_MUSICNOTE) == 52 && H(G::MENU_MUSICNOTE) == 52);
    // scalesWithUI() == false: no UI scale after the override (SKIP: loaded size at uiscale 2); allowsSkin() == false is covered in test 2
    reset(1536, 1536);
    addRes("play-skip.png", 70, 30);
    CHECK(W(G::SKIP) == 70 && H(G::SKIP) == 30);
    // entries without an override are just scaled by the UI scale
    addRes("cursor.png", 64, 32);
    CHECK(W(G::CURSOR) == 128 && H(G::CURSOR) == 64);
    // the plain image of getPlainImage() is processed like the loaded one: builtin hit300 (no skin image) at uiscale 2 -> 2x
    reset(1536, 1536);
    const fs::path emptySkin = newDir("noplain");
    addRes("hit300.png", 12, 12);
    gi::setSkinInfo(skinWith(emptySkin));
    CHECK(gi::getPlainImage(G::HIT_300)->getWidth() == 24);
}

// ---- 6. absentInSkin, optional entries and the legacy name rule ----------------------------------------------------------------------------------------
// The "arrow-warning replaces play-warningarrow" rule is in the caller (Game.java ~894): ARROW_WARNING.getImage(), and only if that is
// null WARNINGARROW.getImage(). GameImage's part is that ARROW_WARNING is optional (null, no error) and WARNINGARROW is a normal entry.
static std::shared_ptr<Image> warningArrow() {
    auto arrow = gi::getImage(GameImage::ARROW_WARNING);
    if (!arrow) arrow = gi::getImage(GameImage::WARNINGARROW);
    return arrow;
}

static void testAbsentAndLegacy() {
    using G = GameImage;
    const fs::path skin = newDir("legacyskin");
    // neither name exists: the optional one is silent, the legacy one reports "Could not find default image"
    reset(); Log::setLogger(&logger);
    CHECK(gi::getImage(G::ARROW_WARNING) == nullptr && logger.errors == 0 && !gi::exists(G::ARROW_WARNING));
    CHECK(gi::getImage(G::WARNINGARROW) == nullptr && logger.errors == 1);
    CHECK(gi::getFrames(G::ARROW_WARNING).empty() && gi::getFirstFrame(G::ARROW_WARNING) == nullptr && gi::getPlainImage(G::ARROW_WARNING) == nullptr);
    CHECK(!gi::isProvidedByGameSkin(G::ARROW_WARNING) && !gi::hasGameSkinImage(G::ARROW_WARNING));
    Log::setLogger(nullptr);
    // only the legacy res/ image: the caller's rule gives it (width 31 = play-warningarrow)
    reset();
    addRes("play-warningarrow.png", 31, 31);
    CHECK(warningArrow() != nullptr && warningArrow()->getWidth() == 31);
    // a skin with arrow-warning: it wins, the legacy image is not used
    reset();
    addRes("play-warningarrow.png", 31, 31); addFile(skin, "arrow-warning.png", 45, 45);
    gi::setSkinInfo(skinWith(skin));
    CHECK(warningArrow()->getWidth() == 45 && gi::isProvidedByGameSkin(G::ARROW_WARNING));
    // a beatmap skin with arrow-warning also wins
    const fs::path bm = newDir("legacybm");
    addFile(bm, "arrow-warning.png", 55, 55);
    CHECK(gi::setBeatmapSkinImage(G::ARROW_WARNING, bm.string()) && warningArrow()->getWidth() == 55);
    // ... and the beatmap skin's legacy name is independent of it
    addFile(bm, "play-warningarrow.png", 66, 66);
    CHECK(gi::setBeatmapSkinImage(G::WARNINGARROW, bm.string()) && gi::getImage(G::WARNINGARROW)->getWidth() == 66);
    // absentInSkin is remembered: a later file is not noticed until clearReferences(); then it is
    reset();
    CHECK(gi::getImage(G::ARROW_WARNING) == nullptr);
    addRes("arrow-warning.png", 40, 40);
    CHECK(gi::getImage(G::ARROW_WARNING) == nullptr && !gi::exists(G::ARROW_WARNING));
    gi::clearReferences();
    CHECK(gi::getImage(G::ARROW_WARNING) != nullptr && gi::getRawWidth(G::ARROW_WARNING) == 40);
    // a beatmap skin can still provide an optional image that is absent everywhere else
    reset();
    CHECK(gi::getImage(G::SLIDER_STARTCIRCLE) == nullptr);
    const fs::path bm2 = newDir("legacybm2");
    addFile(bm2, "sliderstartcircle.png", 25, 25);
    CHECK(gi::setBeatmapSkinImage(G::SLIDER_STARTCIRCLE, bm2.string()) && gi::getImage(G::SLIDER_STARTCIRCLE)->getWidth() == 25);
    CHECK(gi::isProvidedByBeatmapSkin(G::SLIDER_STARTCIRCLE) && !gi::isProvidedByGameSkin(G::SLIDER_STARTCIRCLE));
    // bgLocked entries (scorebar parts) with a beatmap skin that has scorebar-bg: the beatmap skin decides for all parts
    reset();
    addRes("scorebar-bg.png", 50, 10); addRes("scorebar-marker.png", 9, 9);
    const fs::path bm3 = newDir("legacybm3");
    addFile(bm3, "scorebar-bg.png", 60, 12);
    CHECK(gi::setBeatmapSkinImage(G::SCOREBAR_BG, bm3.string()));
    CHECK(gi::getImage(G::SCOREBAR_MARKER) == nullptr);       // getImage() of a bgLocked entry: SCOREBAR_BG has a beatmap image -> the marker's own beatmap image (none)
    CHECK(gi::getDefaultImage(G::SCOREBAR_MARKER) != nullptr);   // the default (res/) marker is still loaded
    gi::destroyBeatmapSkinImage(G::SCOREBAR_BG);
    CHECK(gi::getImage(G::SCOREBAR_MARKER) != nullptr);
}

// ---- 7. lifecycle: init, clearReferences, snapshots, dispose, loadPreloadImages ---------------------------------------------------------------------------
static void testLifecycle() {
    using G = GameImage;
    const fs::path skin = newDir("lifeskin"), skin2 = newDir("lifeskin2"), bm = newDir("lifebm");

    // init() sets the container and uiscale = height / 768 (float); it does not reload or touch loaded images
    gi::init(1920, 1080);
    CHECK(gi::getUIscale() == 1080.f / 768.f);

    // clearReferences() forgets WITHOUT destroying (Java comment); the next getImage() loads a new object at the current uiscale
    reset();
    addRes("cursor.png", 60, 60); addRes("followpoint-0.png", 10, 10);
    auto first = gi::getImage(G::CURSOR);
    auto frames = gi::getImages(G::FOLLOWPOINT);
    CHECK(first && frames.size() == 1 && gi::getRawWidth(G::CURSOR) == 60);
    gi::clearReferences();
    CHECK(!first->isDestroyed() && !frames[0]->isDestroyed());           // still alive: someone else may hold them
    CHECK(gi::getRawWidth(G::CURSOR) == 0 && !gi::hasGameSkinImage(G::CURSOR) && !gi::hasBeatmapSkinImage(G::CURSOR));
    gi::init(1536, 1536);                                                // re-init: the next load uses the new scale
    auto second = gi::getImage(G::CURSOR);
    CHECK(second != first && second->getWidth() == 120 && first->getWidth() == 60);
    CHECK(gi::getImages(G::FOLLOWPOINT)[0] != frames[0] && gi::getImages(G::FOLLOWPOINT)[0]->getWidth() == 20);
    // no stale images: after clearReferences() nobody in GameImage holds the old objects any more
    std::weak_ptr<Image> wFirst = second;
    first.reset(); frames.clear(); second.reset();
    gi::clearReferences();
    CHECK(wFirst.expired());
    // an absent optional entry is forgotten as well
    gi::clearReferences();
    CHECK(gi::getImage(G::ARROW_WARNING) == nullptr);
    addRes("arrow-warning.png", 5, 5);
    gi::clearReferences();
    CHECK(gi::getImage(G::ARROW_WARNING) != nullptr);
    gi::clearReferences();

    // dispose(): destroys the single images (beatmap skin and default) and forgets them; the next getImage() reloads
    reset();
    addRes("cursor.png", 60, 60); addFile(bm, "cursor.png", 90, 90);
    auto def = gi::getDefaultImage(G::CURSOR);
    CHECK(gi::setBeatmapSkinImage(G::CURSOR, bm.string()));
    auto bmImg = gi::getImage(G::CURSOR);
    CHECK(bmImg != def);
    gi::dispose(G::CURSOR);
    CHECK(def->isDestroyed() && bmImg->isDestroyed());
    CHECK(gi::getRawWidth(G::CURSOR) == 0 && !gi::hasBeatmapSkinImage(G::CURSOR));
    auto again = gi::getImage(G::CURSOR);
    CHECK(again && !again->isDestroyed() && again != def && again->getWidth() == 60);
    // like the Java, dispose() leaves animation frame arrays alone
    addRes("followpoint-0.png", 10, 10);
    auto fr = gi::getImages(G::FOLLOWPOINT);
    gi::dispose(G::FOLLOWPOINT);
    CHECK(gi::getImages(G::FOLLOWPOINT).size() == 1 && !fr[0]->isDestroyed());
    gi::clearReferences();

    // snapshots: detach keeps the game skin images (not destroyed), destroys beatmap skin images, empties the entries
    reset();
    addRes("cursor.png", 60, 60); addFile(skin, "cursor.png", 80, 80); addFile(skin2, "cursor.png", 100, 100);
    addRes("hit300-0.png", 10, 10); addRes("hit300-1.png", 10, 10); addRes("hit300.png", 12, 12);
    addFile(bm, "cursor.png", 90, 90);
    gi::setSkinInfo(skinWith(skin));
    auto cur = gi::getImage(G::CURSOR);
    auto judg = gi::getImages(G::HIT_300);
    auto plain = gi::getImage(G::HIT_300);
    CHECK(gi::getImage(G::ARROW_WARNING) == nullptr);                   // optional, absentInSkin
    CHECK(gi::setBeatmapSkinImage(G::CURSOR, bm.string()));
    auto bmCur = gi::getImage(G::CURSOR);
    gi::SnapshotMap snaps = gi::detachLoadedImages();
    CHECK(bmCur->isDestroyed());                                        // beatmap skin images are destroyed first
    CHECK(!cur->isDestroyed() && !judg[0]->isDestroyed() && !plain->isDestroyed());
    CHECK(snaps.size() == 3 && snaps.count(G::CURSOR) && snaps.count(G::HIT_300) && snaps.count(G::ARROW_WARNING));
    CHECK(snaps.at(G::ARROW_WARNING).absentInSkin && snaps.at(G::CURSOR).isSkinned &&
          snaps.at(G::CURSOR).rawWidth == 90);   // Java quirk kept: process() of the beatmap image (90) overwrote rawWidth (80) of the game skin image
    CHECK(gi::getRawWidth(G::CURSOR) == 0 && !gi::hasGameSkinImage(G::CURSOR) && !gi::hasBeatmapSkinImage(G::CURSOR));
    // the entries reload lazily from the skin that is current NOW (a second game skin)
    gi::setSkinInfo(skinWith(skin2));
    CHECK(W(G::CURSOR) == 100);
    // switch back: drop what was loaded for skin2 (destroying it) and attach the stored ones: identity is preserved
    gi::SnapshotMap other = gi::detachLoadedImages();
    auto cur2 = other.at(G::CURSOR).defaultImage;
    gi::setSkinInfo(skinWith(skin));
    gi::attachSnapshots(snaps);
    CHECK(gi::getImage(G::CURSOR) == cur && gi::getImages(G::HIT_300)[0] == judg[0] && gi::getImage(G::HIT_300) == plain);
    CHECK(gi::getRawWidth(G::CURSOR) == 90 && gi::hasGameSkinImage(G::CURSOR) && gi::isProvidedByGameSkin(G::CURSOR));
    CHECK(gi::getImage(G::ARROW_WARNING) == nullptr && !gi::exists(G::ARROW_WARNING));   // absentInSkin was restored: no lookup
    // destroySnapshots(): destroys the held images and empties the map
    gi::destroySnapshots(other);
    CHECK(other.empty() && cur2->isDestroyed() && !cur->isDestroyed());
    gi::SnapshotMap mine = gi::detachLoadedImages();
    gi::destroySnapshots(mine);
    CHECK(mine.empty() && cur->isDestroyed() && judg[0]->isDestroyed() && plain->isDestroyed());
    // an empty detach gives an empty map
    CHECK(gi::detachLoadedImages().empty());

    // loadPreloadImages(): loads the default images of the preload entries only
    reset();
    Log::setLogger(&logger);
    addRes("cursor.png", 60, 60);              // preload = true
    addRes("user0.png", 10, 10);               // USER: preload = false (see table test)
    gi::loadPreloadImages();
    CHECK(gi::getRawWidth(G::CURSOR) == 60);                            // loaded by the call
    CHECK(gi::getRawWidth(G::USER) == 0);                               // not preloaded (loaded on first use)
    CHECK(logger.errors > 0);                                           // preloaded entries that are missing everywhere are reported, the rest carries on
    CHECK(gi::getImages(G::USER).size() == 1 && gi::getImages(G::USER)[0]->getWidth() == 10);   // (rawWidth stays 0 for frame-only entries: Java reads only the single image)
    Log::setLogger(nullptr);

    // repeated lifecycle: no stale images and no growth (also leak-checked: the test runs under ASan/LSan)
    reset();
    addRes("cursor.png", 60, 60); addRes("followpoint-0.png", 10, 10); addRes("followpoint-1.png", 10, 10);
    std::vector<std::weak_ptr<Image>> seen;
    for (int i = 0; i < 20; i++) {
        gi::init(1024 + i, 768);
        seen.push_back(gi::getImage(G::CURSOR));
        for (auto& f : gi::getImages(G::FOLLOWPOINT)) seen.push_back(f);
        gi::SnapshotMap m = gi::detachLoadedImages();
        if (i % 2) gi::destroySnapshots(m);
        gi::clearReferences();
    }
    int alive = 0;
    for (auto& w : seen) if (!w.expired()) alive++;
    CHECK(alive == 0);
    ImageManager::instance().clearAll();
}

int main() {
    dev = std::make_shared<FakeDevice>();
    setRenderDevice(dev);
    root = fs::temp_directory_path() / ("t11_gameimage_" + std::to_string(::getpid()));
    fs::create_directories(root);
    testTable();
    testLookupOrder();
    testHDAndScale();
    testAnimations();
    testProcessOverrides();
    testAbsentAndLegacy();
    testLifecycle();
    gi::clearReferences();
    ImageManager::instance().clearAll();
    fs::remove_all(root);
    setRenderDevice(nullptr);
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("test_gameimage_t11: all passed\n");
    return 0;
}
