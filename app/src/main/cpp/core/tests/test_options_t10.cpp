// T10 milestone 1 tests: GameOption enum (order, defaults, ranges), accessors, setters, toggles, SELECT options,
// read()/write() of single options, group lists, seams, concurrency smoke test.
// No JVM here, so every expected value below is derived by hand from Options.java (see the comments).
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "opsu/audio/MusicController.h"
#include "opsu/audio/SoundController.h"
#include "opsu/beatmap/DownloadImporter.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/options/OptionGroup.h"
#include "opsu/options/Options.h"
#include "opsu/options/OptionsWiring.h"
#include "opsu/util/JavaCompat.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define CHECK_EQ(a, b) do { std::string _a = (a), _b = (b); if (_a != _b) { std::printf("FAIL %s:%d  %s == \"%s\" but expected \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); failures++; } } while (0)

using Id = GameOption::Id;
enum T { T_INTERNAL, T_SELECT, T_BOOLEAN, T_NUMERIC };
struct Row { Id id; T type; const char* key; bool defBool; int defVal, min, max; bool restart; };

// Copied from the Java enum constructors, in declaration order (= order of opsu.cfg).
// INTERNAL = 1-arg constructor (SELECT type, no name), SELECT = 3-arg, BOOLEAN = 4-arg, NUMERIC = 6-arg.
// MOBILE_UI_SCALING / PLAYFIELDSCALE show 0: the Java default depends on the screen size, tested separately.
static const Row kRows[] = {
    {GameOption::Id::NULL_OPTION, T_SELECT, "", false, 0, 0, 0, false},
    {GameOption::Id::BEATMAP_DIRECTORY, T_INTERNAL, "BeatmapDirectory", false, 0, 0, 0, false},
    {GameOption::Id::IMPORT_DIRECTORY, T_INTERNAL, "ImportDirectory", false, 0, 0, 0, false},
    {GameOption::Id::SCREENSHOT_DIRECTORY, T_INTERNAL, "ScreenshotDirectory", false, 0, 0, 0, false},
    {GameOption::Id::REPLAY_DIRECTORY, T_INTERNAL, "ReplayDirectory", false, 0, 0, 0, false},
    {GameOption::Id::SKIN_DIRECTORY, T_INTERNAL, "SkinDirectory", false, 0, 0, 0, false},
    {GameOption::Id::FFMPEG_PATH, T_INTERNAL, "FFmpegPath", false, 0, 0, 0, false},
    {GameOption::Id::THEME_SONG, T_INTERNAL, "ThemeSong", false, 0, 0, 0, false},
    {GameOption::Id::THEME_SONG_TIMINGPOINT, T_INTERNAL, "ThemeSongTiming", false, 0, 0, 0, false},
    {GameOption::Id::SCREEN_RESOLUTION, T_SELECT, "ScreenResolution", false, 0, 0, 0, true},
    {GameOption::Id::FULLSCREEN, T_BOOLEAN, "Fullscreen", false, 0, 0, 0, true},
    {GameOption::Id::SKIN, T_SELECT, "Skin", false, 0, 0, 0, false},
    {GameOption::Id::TARGET_FPS, T_SELECT, "FrameSync", false, 0, 0, 0, false},
    {GameOption::Id::SHOW_FPS, T_BOOLEAN, "FpsCounter", true, 0, 0, 0, false},
    {GameOption::Id::SHOW_UNICODE, T_BOOLEAN, "ShowUnicode", false, 0, 0, 0, false},
    {GameOption::Id::SCREENSHOT_FORMAT, T_SELECT, "ScreenshotFormat", false, 0, 0, 0, false},
    {GameOption::Id::CURSOR_SIZE, T_NUMERIC, "CursorSize", false, 100, 50, 200, false},
    {GameOption::Id::DYNAMIC_BACKGROUND, T_BOOLEAN, "DynamicBackground", true, 0, 0, 0, false},
    {GameOption::Id::SHOW_INTRO, T_BOOLEAN, "ShowIntro", true, 0, 0, 0, false},
    {GameOption::Id::LOAD_VERBOSE, T_BOOLEAN, "LoadVerbose", false, 0, 0, 0, false},
    {GameOption::Id::MASTER_VOLUME, T_NUMERIC, "VolumeUniversal", false, 35, 0, 100, false},
    {GameOption::Id::MUSIC_VOLUME, T_NUMERIC, "VolumeMusic", false, 80, 0, 100, false},
    {GameOption::Id::EFFECT_VOLUME, T_NUMERIC, "VolumeEffect", false, 70, 0, 100, false},
    {GameOption::Id::HITSOUND_VOLUME, T_NUMERIC, "VolumeHitSound", false, 30, 0, 100, false},
    {GameOption::Id::MUSIC_OFFSET, T_NUMERIC, "Offset", false, 0, -500, 500, false},
    {GameOption::Id::RENDER_OFFSET, T_NUMERIC, "RenderOffset", false, 0, -500, 500, false},
    {GameOption::Id::DISABLE_GAMEPLAY_SOUNDS, T_BOOLEAN, "DisableGameplaySound", false, 0, 0, 0, false},
    {GameOption::Id::DISABLE_SOUNDS, T_BOOLEAN, "DisableSound", false, 0, 0, 0, true},
    {GameOption::Id::KEY_LEFT, T_SELECT, "keyOsuLeft", false, 0, 0, 0, false},
    {GameOption::Id::KEY_RIGHT, T_SELECT, "keyOsuRight", false, 0, 0, 0, false},
    {GameOption::Id::DISABLE_MOUSE_WHEEL, T_BOOLEAN, "MouseDisableWheel", false, 0, 0, 0, false},
    {GameOption::Id::DISABLE_MOUSE_BUTTONS, T_BOOLEAN, "MouseDisableButtons", false, 0, 0, 0, false},
    {GameOption::Id::MULTITOUCH_GAMEPLAY, T_BOOLEAN, "MultitouchGameplay", true, 0, 0, 0, false},
    {GameOption::Id::TOUCH_MAX_FINGERS, T_NUMERIC, "TouchMaxFingers", false, 10, 1, 10, false},
    {GameOption::Id::TOUCH_PALM_REJECTION, T_BOOLEAN, "TouchPalmRejection", true, 0, 0, 0, false},
    {GameOption::Id::DISABLE_CURSOR, T_BOOLEAN, "DisableCursor", false, 0, 0, 0, false},
    {GameOption::Id::BACKGROUND_DIM, T_NUMERIC, "DimLevel", false, 50, 0, 100, false},
    {GameOption::Id::BACKGROUND_BLUR, T_NUMERIC, "BlurLevel", false, 0, 0, 100, false},
    {GameOption::Id::FORCE_DEFAULT_PLAYFIELD, T_BOOLEAN, "ForceDefaultPlayfield", false, 0, 0, 0, false},
    {GameOption::Id::ENABLE_STORYBOARD, T_BOOLEAN, "Storyboard", true, 0, 0, 0, false},
    {GameOption::Id::ENABLE_VIDEOS, T_BOOLEAN, "Video", true, 0, 0, 0, false},
    {GameOption::Id::IGNORE_BEATMAP_SKINS, T_BOOLEAN, "IgnoreBeatmapSkins", false, 0, 0, 0, false},
    {GameOption::Id::FORCE_SKIN_CURSOR, T_BOOLEAN, "UseSkinCursor", false, 0, 0, 0, false},
    {GameOption::Id::SNAKING_SLIDERS, T_BOOLEAN, "SnakingSliders", true, 0, 0, 0, false},
    {GameOption::Id::SLIDER_CAPS, T_BOOLEAN, "ExperimentalSliderCaps", false, 0, 0, 0, false},
    {GameOption::Id::SLIDER_SHRINK, T_BOOLEAN, "ExperimentalSliderShrink", true, 0, 0, 0, false},
    {GameOption::Id::SLIDER_MERGE, T_BOOLEAN, "ExperimentalSliderMerge", true, 0, 0, 0, false},
    {GameOption::Id::SHOW_HIT_LIGHTING, T_BOOLEAN, "HitLighting", true, 0, 0, 0, false},
    {GameOption::Id::SHOW_HIT_STARS, T_BOOLEAN, "HitStars", true, 0, 0, 0, false},
    {GameOption::Id::SHOW_COMBO_BURSTS, T_BOOLEAN, "ComboBurst", true, 0, 0, 0, false},
    {GameOption::Id::SHOW_PERFECT_HIT, T_BOOLEAN, "PerfectHit", true, 0, 0, 0, false},
    {GameOption::Id::SHOW_FOLLOW_POINTS, T_BOOLEAN, "FollowPoints", true, 0, 0, 0, false},
    {GameOption::Id::SHOW_HIT_ERROR_BAR, T_BOOLEAN, "ScoreMeter", false, 0, 0, 0, false},
    {GameOption::Id::ALWAYS_SHOW_KEY_OVERLAY, T_BOOLEAN, "KeyOverlay", false, 0, 0, 0, false},
    {GameOption::Id::LOAD_HD_IMAGES, T_BOOLEAN, "LoadHDImages", true, 0, 0, 0, false},
    {GameOption::Id::FIXED_CS, T_NUMERIC, "FixedCS", false, 0, 0, 100, false},
    {GameOption::Id::FIXED_HP, T_NUMERIC, "FixedHP", false, 0, 0, 100, false},
    {GameOption::Id::FIXED_AR, T_NUMERIC, "FixedAR", false, 0, 0, 100, false},
    {GameOption::Id::FIXED_OD, T_NUMERIC, "FixedOD", false, 0, 0, 100, false},
    {GameOption::Id::FIXED_SPEED, T_NUMERIC, "FixedSpeed", false, 0, 0, 300, false},
    {GameOption::Id::CHECKPOINT, T_NUMERIC, "Checkpoint", false, 0, 0, 1800, false},
    {GameOption::Id::SONG_THUMBNAILS, T_BOOLEAN, "SongListThumbnails", true, 0, 0, 0, false},
    {GameOption::Id::PARALLAX, T_BOOLEAN, "MenuParallax", true, 0, 0, 0, false},
    {GameOption::Id::ENABLE_THEME_SONG, T_BOOLEAN, "MenuMusic", true, 0, 0, 0, false},
    {GameOption::Id::REPLAY_SEEKING, T_BOOLEAN, "ReplaySeeking", false, 0, 0, 0, false},
    {GameOption::Id::DISABLE_UPDATER, T_BOOLEAN, "DisableUpdater", false, 0, 0, 0, false},
    {GameOption::Id::ENABLE_WATCH_SERVICE, T_BOOLEAN, "WatchService", false, 0, 0, 0, true},
    {GameOption::Id::IN_GAME_PAUSE, T_BOOLEAN, "InGamePause", false, 0, 0, 0, false},
    {GameOption::Id::PLAYER_LOADER, T_BOOLEAN, "PlayerLoader", true, 0, 0, 0, false},
    {GameOption::Id::MOBILE_UI_SCALING, T_NUMERIC, "MobileUIScale", false, 0, 5, 25, false},
    {GameOption::Id::PLAYFIELDSCALE, T_NUMERIC, "PlayfieldScale", false, 0, 10, 20, false},
    {GameOption::Id::SCOREBOARD, T_BOOLEAN, "Scoreboard", false, 0, 0, 0, false},
    {GameOption::Id::AVGOFFSETDISP, T_BOOLEAN, "AVGOffsetDisp", false, 0, 0, 0, false},
    {GameOption::Id::DISABLE_SPINNER_UI, T_BOOLEAN, "DisableSpinnerUI", false, 0, 0, 0, false},
    {GameOption::Id::DISABLE_SPINNER_SOUND, T_BOOLEAN, "DisableSpinnerSound", false, 0, 0, 0, false},
};
static constexpr int kRowCount = sizeof(kRows) / sizeof(kRows[0]);

static void testEnumAndDefaults() {
    Options::resetForTests();
    CHECK(kRowCount == 75);                 // NULL + 74 real options in Options.java
    CHECK(GameOption::COUNT == kRowCount);
    CHECK(GameOption::values().size() == static_cast<size_t>(kRowCount));
    for (int i = 0; i < kRowCount; i++) {
        const Row& r = kRows[i];
        GameOption o = GameOption::values()[i];
        CHECK(o.ordinal() == i);                       // same order as the Java enum
        CHECK(o.id() == r.id);
        CHECK(static_cast<int>(r.id) == i);
        CHECK_EQ(o.getDisplayName(), r.key);
        CHECK(o.isRestartRequired() == r.restart);
        switch (r.type) {
            case T_INTERNAL:
            case T_SELECT:
                CHECK(o.getType() == GameOption::OptionType::SELECT);
                break;
            case T_BOOLEAN:
                CHECK(o.getType() == GameOption::OptionType::BOOLEAN);
                CHECK(o.getBooleanValue() == r.defBool);
                if (o.getBooleanValue() != r.defBool) std::printf("   (%s)\n", r.key);
                break;
            case T_NUMERIC:
                CHECK(o.getType() == GameOption::OptionType::NUMERIC);
                CHECK(o.getMinValue() == r.min);
                CHECK(o.getMaxValue() == r.max);
                if (o.id() != Id::MOBILE_UI_SCALING && o.id() != Id::PLAYFIELDSCALE) {
                    CHECK(o.getIntegerValue() == r.defVal);
                    CHECK(o.getDefaultIntegerValue() == r.defVal);
                    CHECK(!o.isCustomized());
                }
                break;
        }
        GameOption byName(Id::NULL_OPTION);
        CHECK(GameOption::fromConstantName(o.constantName(), byName) && byName == o);
    }
    // internal options have no name / description (Java: null)
    CHECK(GameOption::BEATMAP_DIRECTORY.getName().empty() && GameOption::THEME_SONG.getDescription().empty());
    CHECK_EQ(GameOption::NULL_OPTION.constantName(), "NULL");
    CHECK_EQ(GameOption::MASTER_VOLUME.getName(), "Master");
    CHECK_EQ(GameOption::LOAD_HD_IMAGES.getDescription(), "Loads HD (@2x) images when available.\nIncreases memory usage and loading times.");
    CHECK_EQ(GameOption::ENABLE_THEME_SONG.getDescription(), "opsu! will play themed music throughout the game, instead of using random beatmaps.");
    // non-option state defaults
    CHECK(Options::getTargetFPS() == 60);
    CHECK_EQ(Options::getScreenshotFormat(), "png");
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.getValueString(), "1024x768");
    CHECK_EQ(Options::getSkinName(), "Default");
    CHECK(Options::getGameKeyLeft() == OptionKeys::KEY_Z && Options::getGameKeyRight() == OptionKeys::KEY_X);
    CHECK(!Options::isDirty());
}

static void testAccessorsDefaults() {
    Options::resetForTests();
    // master 35 -> 35/100f, music 80, effect 70, hitsound 30 (float division like the Java)
    CHECK(Options::getMasterVolume() == 35 / 100.0f);
    CHECK(Options::getMusicVolume() == 80 / 100.0f);
    CHECK(Options::getEffectVolume() == 70 / 100.0f);
    CHECK(Options::getHitSoundVolume() == 30 / 100.0f);
    CHECK(Options::getMusicOffset() == 0 && Options::getRenderOffset() == 0);
    CHECK(Options::getCursorScale() == 1.0f);
    CHECK(Options::getBackgroundDim() == (100 - 50) / 100.0f);   // stored dim 50 -> alpha 0.5
    CHECK(Options::getBackgroundBlur() == 0);
    CHECK(Options::getFixedCS() == 0.0f && Options::getFixedSpeed() == 0.0f);
    CHECK(Options::getCheckpoint() == 0);
    CHECK(Options::getMaxTouchFingers() == 10);
    CHECK(Options::isSliderSnaking() && Options::isSliderShrinking() && Options::isSliderMerging() && !Options::isSliderCapsDrawn());
    CHECK(Options::isHitLightingEnabled() && Options::isHitStarsEnabled() && Options::isComboBurstEnabled());
    CHECK(!Options::useUnicodeMetadata() && !Options::isSoundDisabled() && !Options::isFullscreen());
    CHECK(Options::isFPSCounterEnabled());
    CHECK(Options::isThemeSongEnabled() && Options::isStoryboardEnabled() && Options::isBeatmapVideoEnabled());
    CHECK(Options::loadHDImages() && Options::isPlayerLoaderEnabled() && !Options::isInGamePauseEnabled());
    // no screen-width seam: treated as a big screen -> UI scale 10 (1.0), playfield 10 (1.0)
    CHECK(Options::getMobileUIScale() == 1.0f && Options::getPlayfieldScale() == 1.0f);
    // getMobileUIScale(scale) = 1 + ((ui - 1) * scale); getMobileUIScaleHigh: ui > 1 ? ui : getMobileUIScale(0.5f)
    CHECK(Options::getMobileUIScale(0.5f) == 1.0f && Options::getMobileUIScaleHigh() == 1.0f);
    GameOption::MOBILE_UI_SCALING.setValue(20);
    CHECK(Options::getMobileUIScale() == 2.0f);
    CHECK(Options::getMobileUIScale(0.5f) == 1.5f);   // 1 + (2 - 1) * 0.5
    CHECK(Options::getMobileUIScaleHigh() == 2.0f);
    GameOption::MOBILE_UI_SCALING.setValue(5);        // 0.5: not > 1 -> 1 + (0.5 - 1) * 0.5 = 0.75
    CHECK(Options::getMobileUIScaleHigh() == 0.75f);
}

static void testSettersClamp() {
    Options::resetForTests();
    CHECK(!Options::isDirty());
    GameOption::MASTER_VOLUME.setValue(150);           // clamp to [0, 100]
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 100);
    CHECK(Options::isDirty());                         // setValue marks dirty
    GameOption::MASTER_VOLUME.setValue(-3);
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 0);
    GameOption::MUSIC_OFFSET.setValue(-900);           // [-500, 500]
    CHECK(Options::getMusicOffset() == -500);
    GameOption::MUSIC_OFFSET.setValue(501);
    CHECK(Options::getMusicOffset() == 500);
    GameOption::TOUCH_MAX_FINGERS.setValue(0);         // [1, 10]
    CHECK(Options::getMaxTouchFingers() == 1);
    GameOption::CURSOR_SIZE.setValue(10);              // [50, 200]
    CHECK(Options::getCursorScale() == 0.5f);
    GameOption::CURSOR_SIZE.setValue(999);
    CHECK(Options::getCursorScale() == 2.0f);
    GameOption::FIXED_SPEED.setValue(300);             // [0, 300]
    CHECK(Options::getFixedSpeed() == 3.0f);
    GameOption::MOBILE_UI_SCALING.setValue(100);       // [5, 25]
    CHECK(GameOption::MOBILE_UI_SCALING.getIntegerValue() == 25);
    GameOption::PLAYFIELDSCALE.setValue(1);            // [10, 20]
    CHECK(GameOption::PLAYFIELDSCALE.getIntegerValue() == 10);
    // customized / reset
    CHECK(GameOption::CURSOR_SIZE.isCustomized());
    GameOption::CURSOR_SIZE.resetToDefault();
    CHECK(GameOption::CURSOR_SIZE.getIntegerValue() == 100 && !GameOption::CURSOR_SIZE.isCustomized());
    GameOption::SHOW_INTRO.resetToDefault();           // no-op for booleans
    CHECK(Options::isIntroEnabled());
    // Options.setCheckpoint: time in [0, 3600) accepted, then clamped to the option max (1800)
    CHECK(Options::setCheckpoint(125) && GameOption::CHECKPOINT.getIntegerValue() == 125);
    CHECK(Options::setCheckpoint(3000) && GameOption::CHECKPOINT.getIntegerValue() == 1800);
    CHECK(!Options::setCheckpoint(3600) && !Options::setCheckpoint(-1));
    CHECK(GameOption::CHECKPOINT.getIntegerValue() == 1800);
    CHECK(Options::getCheckpoint() == 1800000);        // seconds -> ms
    // setMasterVolume: only [0, 1]; (int)(v * 100f)
    Options::setMasterVolume(nullptr, 0.5f);
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 50);
    Options::setMasterVolume(nullptr, 1.5f);
    Options::setMasterVolume(nullptr, -0.1f);
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 50);
    Options::setMasterVolume(nullptr, 1.0f);
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 100);
    // boolean setter
    GameOption::SHOW_INTRO.setValue(false);
    CHECK(!Options::isIntroEnabled());
}

struct FakeContainer : IGameContainer {
    int fps = 0; bool vsync = false; int calls = 0;
    void setTargetFrameRate(int f) override { fps = f; calls++; }
    void setVSync(bool v) override { vsync = v; }
};

static void testToggleAndSelect() {
    Options::resetForTests();
    std::vector<std::string> notes;
    int fpsResets = 0, glyphLoads = 0;
    OptionsSeams s;
    s.sendBarNotification = [&](const std::string& m) { notes.push_back(m); };
    s.resetFPSDisplay = [&] { fpsResets++; };
    s.loadUnicodeGlyphs = [&] { glyphLoads++; };
    Options::setSeams(s);

    // toggle() flips the boolean and, like the Java, does NOT mark dirty
    CHECK(!Options::isDirty());
    GameOption::SHOW_INTRO.toggle(nullptr);
    CHECK(!Options::isIntroEnabled() && !Options::isDirty());
    GameOption::SHOW_INTRO.toggle(nullptr);
    CHECK(Options::isIntroEnabled());
    // SHOW_FPS: toggle + UI.resetFPSDisplay()
    Options::toggleFPSCounter();
    CHECK(!Options::isFPSCounterEnabled() && fpsResets == 1);
    // SHOW_UNICODE: glyphs only loaded when turned ON
    GameOption::SHOW_UNICODE.toggle(nullptr);
    CHECK(Options::useUnicodeMetadata() && glyphLoads == 1);
    GameOption::SHOW_UNICODE.toggle(nullptr);
    CHECK(!Options::useUnicodeMetadata() && glyphLoads == 1);
    // FULLSCREEN: hasFullscreenDisplayMode() is always false in this Java -> notification, value unchanged
    GameOption::FULLSCREEN.toggle(nullptr);
    CHECK(!Options::isFullscreen());
    CHECK(notes.size() == 1 && notes[0] == "Fullscreen mode is not available at resolution 1024x768");
    // toggleMouseDisabled sends "Mouse buttons are disabled." / "...enabled."
    Options::toggleMouseDisabled();
    CHECK(Options::isMouseDisabled() && notes.back() == "Mouse buttons are disabled.");
    Options::toggleMouseDisabled();
    CHECK(!Options::isMouseDisabled() && notes.back() == "Mouse buttons are enabled.");

    // TARGET_FPS: list is {60, 120, 240, Unlimited}
    auto items = GameOption::TARGET_FPS.getItemList();
    CHECK(items.size() == 4);
    CHECK_EQ(items[0], "60fps (vsync)"); CHECK_EQ(items[1], "120fps"); CHECK_EQ(items[2], "240fps"); CHECK_EQ(items[3], "Unlimited");
    FakeContainer c;
    GameOption::TARGET_FPS.selectItem(1, &c);
    CHECK(Options::getTargetFPS() == 120 && c.fps == 120 && !c.vsync);
    GameOption::TARGET_FPS.selectItem(0, &c);
    CHECK(Options::getTargetFPS() == 60 && c.fps == 60 && c.vsync);   // vsync only at 60
    GameOption::TARGET_FPS.selectItem(3, &c);
    CHECK(Options::getTargetFPS() == -1 && c.fps == -1);
    CHECK_EQ(GameOption::TARGET_FPS.getValueString(), "Unlimited");
    GameOption::TARGET_FPS.selectItem(9, &c);                          // out of range: ignored (Java would throw later)
    CHECK(Options::getTargetFPS() == -1);
    // setNextFPS cycles 60 -> 120 -> 240 -> 60 (Unlimited is skipped) and notifies
    GameOption::TARGET_FPS.selectItem(0, &c);
    Options::setNextFPS(&c);
    CHECK(Options::getTargetFPS() == 120 && notes.back() == "Frame limiter: 120fps");
    Options::setNextFPS(&c);
    CHECK(Options::getTargetFPS() == 240);
    Options::setNextFPS(&c);
    CHECK(Options::getTargetFPS() == 60 && notes.back() == "Frame limiter: 60fps (vsync)");
    // from Unlimited: (3 + 1) % 4 = 0 -> 60
    GameOption::TARGET_FPS.selectItem(3, &c);
    Options::setNextFPS(&c);
    CHECK(Options::getTargetFPS() == 60);

    // SCREENSHOT_FORMAT
    auto sf = GameOption::SCREENSHOT_FORMAT.getItemList();
    CHECK(sf.size() == 3 && sf[0] == "PNG" && sf[1] == "JPG" && sf[2] == "BMP");
    GameOption::SCREENSHOT_FORMAT.selectItem(1);
    CHECK_EQ(Options::getScreenshotFormat(), "jpg");
    CHECK_EQ(GameOption::SCREENSHOT_FORMAT.getValueString(), "JPG");
    GameOption::SCREENSHOT_FORMAT.selectItem(7);   // ignored
    CHECK_EQ(Options::getScreenshotFormat(), "jpg");

    // SCREEN_RESOLUTION list: only 800x600 plus the sizes that fit the desktop (default seam: 0x0 -> only 800x600)
    auto rl = GameOption::SCREEN_RESOLUTION.getItemList();
    CHECK(rl.size() == 1 && rl[0] == "800x600");
}

static void testResolutionList() {
    Options::resetForTests();
    OptionsSeams s;
    s.desktopDisplaySize = [] { return std::make_pair(1366, 768); };
    Options::setSeams(s);
    // fits 1366x768: 800x600, 1024x600, 1024x768, 1280x720, 1280x800? (800 > 768: no), 1366x768 -> see below
    auto rl = GameOption::SCREEN_RESOLUTION.getItemList();
    std::vector<std::string> expect = {"800x600", "1024x600", "1024x768", "1280x720", "1366x768"};
    CHECK(rl == expect);
    GameOption::SCREEN_RESOLUTION.selectItem(3);
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.getValueString(), "1280x720");
    CHECK(Options::getResolution().getWidth() == 1280 && Options::getResolution().getHeight() == 720);
    GameOption::SCREEN_RESOLUTION.selectItem(5);   // out of range
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.getValueString(), "1280x720");

    struct Host : IDisplayHost {
        int sw = 1366, sh = 768, w = 0, h = 0; bool fs = true;
        int getScreenWidth() override { return sw; }
        int getScreenHeight() override { return sh; }
        void setDisplayMode(int ww, int hh, bool f) override { w = ww; h = hh; fs = f; }
    } host;
    Options::setDisplayMode(host);                  // fits: keeps 1280x720; fullscreen off anyway
    CHECK(host.w == 1280 && host.h == 720 && !host.fs);
    host.sw = 1024; host.sh = 600;                  // smaller than the resolution -> falls back to 800x600
    Options::setDisplayMode(host);
    CHECK(host.w == 800 && host.h == 600);
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.getValueString(), "800x600");
}

static void testVolumeSeams() {
    Options::resetForTests();
    std::vector<float> adjusted, ctl;
    OptionsSeams s;
    s.onVolumeAdjusted = [&](float v) { adjusted.push_back(v); };
    s.setMusicControllerVolume = [&](float v) { ctl.push_back(v); };
    Options::setSeams(s);
    GameOption::MASTER_VOLUME.setValue(50);            // 0.5f * 0.8f
    CHECK(adjusted.size() == 1 && adjusted[0] == 0.5f * 0.8f);
    GameOption::MUSIC_VOLUME.setValue(100);            // 0.5f * 1.0f
    CHECK(adjusted.size() == 2 && adjusted[1] == 0.5f * 1.0f);
    GameOption::EFFECT_VOLUME.setValue(10);            // effect volume does not touch the music
    CHECK(adjusted.size() == 2);
    GameOption::MASTER_VOLUME.resetToDefault();        // resetToDefault goes through setValue -> seam fires
    CHECK(adjusted.size() == 3 && adjusted[2] == 0.35f * 1.0f);
    Options::setMasterVolume(nullptr, 0.25f);          // 25 -> setValue (seam) + MusicController.setVolume
    CHECK(adjusted.size() == 4 && ctl.size() == 1 && ctl[0] == 0.25f * 1.0f);
    Options::setMasterVolume(nullptr, 2.0f);           // rejected: no calls
    CHECK(adjusted.size() == 4 && ctl.size() == 1);
    // read() (config file) does not fire the seam: the Java assigns the field directly
    GameOption::MASTER_VOLUME.read("77");
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 77 && adjusted.size() == 4);
}

static void testScreenDependentDefaults() {
    Options::resetForTests();
    OptionsSeams s;
    s.screenWidthInches = [] { return 5.5f; };          // <= 6.0 inches: phone
    Options::setSeams(s);
    CHECK(GameOption::MOBILE_UI_SCALING.getIntegerValue() == 20 && GameOption::MOBILE_UI_SCALING.getDefaultIntegerValue() == 20);
    CHECK(GameOption::PLAYFIELDSCALE.getIntegerValue() == 16 && GameOption::PLAYFIELDSCALE.getDefaultIntegerValue() == 16);
    CHECK(Options::getMobileUIScale() == 2.0f && Options::getPlayfieldScale() == 1.6f);
    s.screenWidthInches = [] { return 6.0f; };          // exactly 6.0 is still "<= 6.0f"
    Options::setSeams(s);
    CHECK(GameOption::MOBILE_UI_SCALING.getIntegerValue() == 20);
    s.screenWidthInches = [] { return 6.01f; };
    Options::setSeams(s);
    CHECK(GameOption::MOBILE_UI_SCALING.getIntegerValue() == 10 && GameOption::PLAYFIELDSCALE.getIntegerValue() == 10);
    // a customised value survives a seam change
    GameOption::MOBILE_UI_SCALING.setValue(13);
    s.screenWidthInches = [] { return 4.0f; };
    Options::setSeams(s);
    CHECK(GameOption::MOBILE_UI_SCALING.getIntegerValue() == 13 && GameOption::MOBILE_UI_SCALING.getDefaultIntegerValue() == 20);
    GameOption::MOBILE_UI_SCALING.resetToDefault();
    CHECK(GameOption::MOBILE_UI_SCALING.getIntegerValue() == 20);
}

static void testKeys() {
    Options::resetForTests();
    using namespace OptionKeys;
    CHECK(Options::getGameKeyLeft() == KEY_Z && Options::getGameKeyRight() == KEY_X);
    CHECK(!Options::setGameKeyLeft(KEY_X));             // same as right key
    CHECK(!Options::setGameKeyRight(KEY_Z));            // same as left key
    CHECK(Options::setGameKeyLeft(29));                 // 'A'
    CHECK(Options::setGameKeyRight(KEY_Z));             // now free
    for (int bad : {KEY_NONE, -1, ANDROID_BACK, MOUSE_BACK_BUTTON, KEY_ESCAPE, KEY_SPACE, KEY_UP, KEY_DOWN, KEY_F7, KEY_F10, KEY_F12}) {
        CHECK(!Options::setGameKeyLeft(bad));
    }
    CHECK(Options::getGameKeyLeft() == 29 && Options::getGameKeyRight() == KEY_Z);
    CHECK_EQ(Options::keyName(29), "A");
    CHECK_EQ(Options::keyName(KEY_Z), "Z");
    CHECK_EQ(Options::keyName(0), "Null");               // Keyboard.getKeyName: keycode > 0 ? ... : "Null"
    CHECK_EQ(Options::keyName(8), "1");                  // NUM_1 prints as "1"
    CHECK(Options::keyIndex("Q") == 45 && Options::keyIndex("nonsense") == -1);
    CHECK_EQ(GameOption::KEY_LEFT.write(), "A");
    CHECK_EQ(GameOption::KEY_RIGHT.getValueString(), "Z");
    GameOption::KEY_LEFT.read("B");                      // setGameKeyLeft(getKeyIndex("B"))
    CHECK(Options::getGameKeyLeft() == 30);
    GameOption::KEY_LEFT.read("Z");                      // equals right key: rejected, stays B
    CHECK(Options::getGameKeyLeft() == 30);
    GameOption::KEY_LEFT.read("not-a-key");              // -1 rejected
    CHECK(Options::getGameKeyLeft() == 30);
    // seam override
    OptionsSeams s;
    s.keyName = [](int k) { return "K" + std::to_string(k); };
    s.keyIndex = [](const std::string& n) { return n == "K99" ? 99 : -1; };
    Options::setSeams(s);
    CHECK_EQ(Options::keyName(5), "K5");
    GameOption::KEY_RIGHT.read("K99");
    CHECK(Options::getGameKeyRight() == 99);
}

static void testValueStringsAndWrite() {
    Options::resetForTests();
    // defaults: numeric "%d%%", boolean Yes/No, write = Integer.toString / Boolean.toString
    CHECK_EQ(GameOption::MASTER_VOLUME.getValueString(), "35%");
    CHECK_EQ(GameOption::MASTER_VOLUME.write(), "35");
    CHECK_EQ(GameOption::SHOW_INTRO.getValueString(), "Yes");
    CHECK_EQ(GameOption::SHOW_INTRO.write(), "true");
    CHECK_EQ(GameOption::SLIDER_CAPS.getValueString(), "No");
    CHECK_EQ(GameOption::SLIDER_CAPS.write(), "false");
    CHECK_EQ(GameOption::BACKGROUND_DIM.write(), "50");
    // CURSOR_SIZE: "%.2fx" display, "%.2f" (Locale.US) file
    CHECK_EQ(GameOption::CURSOR_SIZE.getValueString(), "1.00x");
    CHECK_EQ(GameOption::CURSOR_SIZE.write(), "1.00");
    GameOption::CURSOR_SIZE.setValue(150);
    CHECK_EQ(GameOption::CURSOR_SIZE.write(), "1.50");
    // MUSIC_OFFSET "%d ms", RENDER_OFFSET "%dms"; the file has the plain integer
    GameOption::MUSIC_OFFSET.setValue(-30);
    CHECK_EQ(GameOption::MUSIC_OFFSET.getValueString(), "-30 ms");
    CHECK_EQ(GameOption::MUSIC_OFFSET.write(), "-30");
    GameOption::RENDER_OFFSET.setValue(12);
    CHECK_EQ(GameOption::RENDER_OFFSET.getValueString(), "12ms");
    CHECK_EQ(GameOption::TOUCH_MAX_FINGERS.getValueString(), "10");
    // FIXED_*: 0 -> "Disabled" (display), "0.0" in the file; else val / 10f with one decimal
    CHECK_EQ(GameOption::FIXED_CS.getValueString(), "Disabled");
    CHECK_EQ(GameOption::FIXED_CS.write(), "0.0");
    GameOption::FIXED_AR.setValue(75);
    CHECK_EQ(GameOption::FIXED_AR.getValueString(), "7.5");
    CHECK_EQ(GameOption::FIXED_AR.write(), "7.5");
    GameOption::FIXED_OD.setValue(100);
    CHECK_EQ(GameOption::FIXED_OD.write(), "10.0");
    CHECK_EQ(GameOption::FIXED_SPEED.getValueString(), "Disabled");
    CHECK_EQ(GameOption::FIXED_SPEED.write(), "0.00");
    GameOption::FIXED_SPEED.setValue(125);
    CHECK_EQ(GameOption::FIXED_SPEED.getValueString(), "1.25x");
    CHECK_EQ(GameOption::FIXED_SPEED.write(), "1.25");
    // MOBILE_UI_SCALING / PLAYFIELDSCALE: "%.1f"
    CHECK_EQ(GameOption::MOBILE_UI_SCALING.write(), "1.0");
    GameOption::PLAYFIELDSCALE.setValue(16);
    CHECK_EQ(GameOption::PLAYFIELDSCALE.getValueString(), "1.6");
    CHECK_EQ(GameOption::PLAYFIELDSCALE.write(), "1.6");
    // CHECKPOINT: 0 -> Disabled; else "%02d:%02d" (minutes, seconds); the file has the seconds
    CHECK_EQ(GameOption::CHECKPOINT.getValueString(), "Disabled");
    GameOption::CHECKPOINT.setValue(125);
    CHECK_EQ(GameOption::CHECKPOINT.getValueString(), "02:05");
    CHECK_EQ(GameOption::CHECKPOINT.write(), "125");
    GameOption::CHECKPOINT.setValue(1800);
    CHECK_EQ(GameOption::CHECKPOINT.getValueString(), "30:00");
    GameOption::CHECKPOINT.setValue(59);
    CHECK_EQ(GameOption::CHECKPOINT.getValueString(), "00:59");
    // SELECT options
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.write(), "1024x768");
    CHECK_EQ(GameOption::SKIN.write(), "Default");
    CHECK_EQ(GameOption::TARGET_FPS.write(), "60");           // the fps itself, not the index
    CHECK_EQ(GameOption::SCREENSHOT_FORMAT.write(), "0");     // the index
    CHECK_EQ(GameOption::THEME_SONG.write(), "theme.mp3,Rainbows,Kevin MacLeod,219350");
    CHECK_EQ(GameOption::THEME_SONG_TIMINGPOINT.write(), "1120,545.454545454545,4,1,0,100,0,0");
    CHECK_EQ(GameOption::FFMPEG_PATH.write(), "");
}

static void testRead() {
    Options::resetForTests();
    // numeric: in range -> set, out of range -> ignored (not clamped!)
    GameOption::MASTER_VOLUME.read("80");
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 80);
    GameOption::MASTER_VOLUME.read("101");
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 80);
    GameOption::MASTER_VOLUME.read("-1");
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 80);
    GameOption::MUSIC_OFFSET.read("-500");
    CHECK(Options::getMusicOffset() == -500);
    // Integer.parseInt: no whitespace, no decimals -> NumberFormatException; value untouched
    bool threw = false;
    try { GameOption::MASTER_VOLUME.read("12.5"); } catch (const java::NumberFormatException&) { threw = true; }
    CHECK(threw && GameOption::MASTER_VOLUME.getIntegerValue() == 80);
    threw = false;
    try { GameOption::MASTER_VOLUME.read("abc"); } catch (const java::NumberFormatException&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { GameOption::MASTER_VOLUME.read(""); } catch (const java::NumberFormatException&) { threw = true; }
    CHECK(threw);
    // boolean: Boolean.parseBoolean = equalsIgnoreCase("true"), anything else is false (no exception)
    GameOption::SLIDER_CAPS.read("TRUE");
    CHECK(Options::isSliderCapsDrawn());
    GameOption::SLIDER_CAPS.read("True");
    CHECK(Options::isSliderCapsDrawn());
    GameOption::SLIDER_CAPS.read("yes");
    CHECK(!Options::isSliderCapsDrawn());
    GameOption::SLIDER_CAPS.read("true");
    GameOption::SLIDER_CAPS.read("1");
    CHECK(!Options::isSliderCapsDrawn());
    // read() does not mark dirty
    CHECK(!Options::isDirty());
    // CURSOR_SIZE: (int)(Float.parseFloat(s) * 100f) within [50, 200]
    GameOption::CURSOR_SIZE.read("1.5");
    CHECK(GameOption::CURSOR_SIZE.getIntegerValue() == 150);
    GameOption::CURSOR_SIZE.read("3.0");                  // 300 > 200: ignored
    CHECK(GameOption::CURSOR_SIZE.getIntegerValue() == 150);
    GameOption::CURSOR_SIZE.read("0.25");                 // 25 < 50: ignored
    CHECK(GameOption::CURSOR_SIZE.getIntegerValue() == 150);
    GameOption::CURSOR_SIZE.read("2");                    // Float.parseFloat("2") is fine
    CHECK(GameOption::CURSOR_SIZE.getIntegerValue() == 200);
    GameOption::CURSOR_SIZE.read("0.5f");                 // Float.parseFloat accepts an f suffix
    CHECK(GameOption::CURSOR_SIZE.getIntegerValue() == 50);
    threw = false;
    try { GameOption::CURSOR_SIZE.read("big"); } catch (const java::NumberFormatException&) { threw = true; }
    CHECK(threw && GameOption::CURSOR_SIZE.getIntegerValue() == 50);
    GameOption::CURSOR_SIZE.read("NaN");                  // (int) NaN == 0: out of range, ignored
    CHECK(GameOption::CURSOR_SIZE.getIntegerValue() == 50);
    // FIXED_*: * 10f, range [0, 100]
    GameOption::FIXED_CS.read("4.5");
    CHECK(GameOption::FIXED_CS.getIntegerValue() == 45 && Options::getFixedCS() == 4.5f);
    GameOption::FIXED_HP.read("10.0");
    CHECK(GameOption::FIXED_HP.getIntegerValue() == 100);
    GameOption::FIXED_HP.read("10.5");                    // 105 > 100: ignored
    CHECK(GameOption::FIXED_HP.getIntegerValue() == 100);
    GameOption::FIXED_AR.read("0.0");
    CHECK(GameOption::FIXED_AR.getIntegerValue() == 0);
    GameOption::FIXED_SPEED.read("1.25");                 // * 100f -> 125, range [0, 300]
    CHECK(GameOption::FIXED_SPEED.getIntegerValue() == 125);
    GameOption::FIXED_SPEED.read("3.5");
    CHECK(GameOption::FIXED_SPEED.getIntegerValue() == 125);
    // MOBILE_UI_SCALING [5, 25] and PLAYFIELDSCALE [10, 20] (the Java hard-codes the same bounds)
    GameOption::MOBILE_UI_SCALING.read("2.5");
    CHECK(GameOption::MOBILE_UI_SCALING.getIntegerValue() == 25);
    GameOption::MOBILE_UI_SCALING.read("0.4");
    CHECK(GameOption::MOBILE_UI_SCALING.getIntegerValue() == 25);
    GameOption::PLAYFIELDSCALE.read("2.0");
    CHECK(GameOption::PLAYFIELDSCALE.getIntegerValue() == 20);
    GameOption::PLAYFIELDSCALE.read("2.1");
    CHECK(GameOption::PLAYFIELDSCALE.getIntegerValue() == 20);
    // TARGET_FPS: the value is the fps; an unknown fps leaves the index alone
    GameOption::TARGET_FPS.read("240");
    CHECK(Options::getTargetFPS() == 240);
    GameOption::TARGET_FPS.read("-1");
    CHECK(Options::getTargetFPS() == -1);
    GameOption::TARGET_FPS.read("75");
    CHECK(Options::getTargetFPS() == -1);
    threw = false;
    try { GameOption::TARGET_FPS.read("fast"); } catch (const java::NumberFormatException&) { threw = true; }
    CHECK(threw);
    // SCREENSHOT_FORMAT: an index in range
    GameOption::SCREENSHOT_FORMAT.read("2");
    CHECK_EQ(Options::getScreenshotFormat(), "bmp");
    GameOption::SCREENSHOT_FORMAT.read("3");
    CHECK_EQ(Options::getScreenshotFormat(), "bmp");
    GameOption::SCREENSHOT_FORMAT.read("-1");
    CHECK_EQ(Options::getScreenshotFormat(), "bmp");
    // SCREEN_RESOLUTION: "WxH" -> RES_W_H; unknown names ignored (IllegalArgumentException swallowed)
    GameOption::SCREEN_RESOLUTION.read("1920x1080");
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.getValueString(), "1920x1080");
    GameOption::SCREEN_RESOLUTION.read("1234x567");
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.getValueString(), "1920x1080");
    GameOption::SCREEN_RESOLUTION.read("garbage");
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.getValueString(), "1920x1080");
    // SKIN: stored as is (even if the directory does not exist)
    GameOption::SKIN.read("My Skin");
    CHECK_EQ(Options::getSkinName(), "My Skin");
    // THEME_SONG_TIMINGPOINT: must parse as a TimingPoint, else logged and ignored
    GameOption::THEME_SONG_TIMINGPOINT.read("0,500.0,4,1,0,100,0,0");
    CHECK_EQ(GameOption::THEME_SONG_TIMINGPOINT.write(), "0,500.0,4,1,0,100,0,0");
    GameOption::THEME_SONG_TIMINGPOINT.read("nonsense");
    CHECK_EQ(GameOption::THEME_SONG_TIMINGPOINT.write(), "0,500.0,4,1,0,100,0,0");
    // THEME_SONG: malformed (not 4 fields / bad length) -> old value kept; audio must exist
    GameOption::THEME_SONG.read("a.mp3,T,A");
    CHECK_EQ(GameOption::THEME_SONG.write(), "theme.mp3,Rainbows,Kevin MacLeod,219350");
    GameOption::THEME_SONG.read("a.mp3,T,A,notanumber");
    CHECK_EQ(GameOption::THEME_SONG.write(), "theme.mp3,Rainbows,Kevin MacLeod,219350");
    GameOption::THEME_SONG.read("missing-file.mp3,T,A,1000");     // not a file, no bundled resource -> rejected
    CHECK_EQ(GameOption::THEME_SONG.write(), "theme.mp3,Rainbows,Kevin MacLeod,219350");
    OptionsSeams s;
    s.bundledResourceExists = [](const std::string& n) { return n == "other.mp3"; };
    Options::setSeams(s);
    GameOption::THEME_SONG.read("other.mp3,Title,Artist,1234");   // ResourceLoader.resourceExists("other.mp3")
    CHECK_EQ(GameOption::THEME_SONG.write(), "other.mp3,Title,Artist,1234");
    // FFMPEG_PATH: empty string leaves it null
    GameOption::FFMPEG_PATH.read("");
    CHECK_EQ(GameOption::FFMPEG_PATH.write(), "");
    // NULL_OPTION: SELECT with nothing to read
    GameOption::NULL_OPTION.read("x");
}

static void testMatches() {
    CHECK(GameOption::MASTER_VOLUME.matches("master"));
    CHECK(GameOption::MASTER_VOLUME.matches("global"));          // description "Global volume level."
    CHECK(!GameOption::MASTER_VOLUME.matches(""));               // empty query never matches
    CHECK(!GameOption::MASTER_VOLUME.matches("storyboard"));
    CHECK(GameOption::ENABLE_STORYBOARD.matches("storyboard"));
    CHECK(!GameOption::BEATMAP_DIRECTORY.matches("beatmap"));    // internal: no name/description
    GameOption::MASTER_VOLUME.setVisible(false);
    CHECK(!GameOption::MASTER_VOLUME.isVisible() && GameOption::MUSIC_VOLUME.isVisible());
    GameOption::MASTER_VOLUME.setVisible(true);
}

static std::vector<std::string> names(const std::vector<GameOption>& v) {
    std::vector<std::string> r;
    for (auto& o : v) r.push_back(o.constantName());
    return r;
}

static void testGroups() {
    Options::resetForTests();
    OptionGroup::rebuild();
    // Android / default (isDesktop seam false): SCREEN_RESOLUTION and TARGET_FPS are dropped by desktopOnly().
    {
        auto& all = OptionGroup::allOptions();
        // 7 section headers + sub-groups: General 2, Graphics 5, Gameplay 3, Audio 3, Skin 2, Input 3, Custom 2 = 20
        CHECK(all.size() == 27);
        CHECK_EQ(all[0].getName(), "General");
        CHECK(all[0].hasIcon() && all[0].getIconKind() == OptionGroup::IconKind::GENERAL && all[0].getOptions().empty());
        CHECK_EQ(all[1].getName(), "LANGUAGE");
        CHECK(!all[1].hasIcon());
        CHECK(names(all[1].getOptions()) == std::vector<std::string>{"SHOW_UNICODE"});
        CHECK(names(all[2].getOptions()) == (std::vector<std::string>{"SHOW_INTRO", "LOAD_VERBOSE"}));
        CHECK_EQ(all[3].getName(), "Graphics");
        CHECK_EQ(all[4].getName(), "LAYOUT");
        CHECK(names(all[4].getOptions()) == std::vector<std::string>{"MOBILE_UI_SCALING"});   // SCREEN_RESOLUTION dropped
        CHECK_EQ(all[5].getName(), "RENDERER");
        CHECK(names(all[5].getOptions()) == std::vector<std::string>{"SHOW_FPS"});             // TARGET_FPS dropped
        CHECK_EQ(all[6].getName(), "DETAIL SETTINGS");
        CHECK(all[6].getOptions().size() == 7 && all[6].getOption(0) == GameOption::SNAKING_SLIDERS &&
              all[6].getOption(6) == GameOption::SONG_THUMBNAILS);
        CHECK_EQ(all[7].getName(), "SLIDERS");
        CHECK(names(all[7].getOptions()) == (std::vector<std::string>{"SLIDER_MERGE", "SLIDER_SHRINK", "SLIDER_CAPS"}));
        CHECK_EQ(all[8].getName(), "MAIN MENU");
        CHECK_EQ(all[9].getName(), "Gameplay");
        CHECK_EQ(all[10].getName(), "GENERAL");
        CHECK(all[10].getOptions().size() == 8);
        CHECK_EQ(all[11].getName(), "ON-SCREEN DISPLAY");
        CHECK(names(all[11].getOptions()) ==
              (std::vector<std::string>{"SHOW_HIT_ERROR_BAR", "AVGOFFSETDISP", "ALWAYS_SHOW_KEY_OVERLAY", "SCOREBOARD"}));
        CHECK_EQ(all[12].getName(), "SPINNERS");
        CHECK_EQ(all[13].getName(), "Audio");
        CHECK(names(all[14].getOptions()) == (std::vector<std::string>{"MASTER_VOLUME", "MUSIC_VOLUME", "EFFECT_VOLUME", "HITSOUND_VOLUME"}));
        CHECK_EQ(all[16].getName(), "OFFSET ADJUSTMENT");
        CHECK(names(all[16].getOptions()) == std::vector<std::string>{"MUSIC_OFFSET"});        // RENDER_OFFSET is commented out
        CHECK_EQ(all[17].getName(), "Skin");
        CHECK(names(all[18].getOptions()) == (std::vector<std::string>{"SKIN", "LOAD_HD_IMAGES", "IGNORE_BEATMAP_SKINS"}));
        CHECK(names(all[19].getOptions()) == (std::vector<std::string>{"FORCE_SKIN_CURSOR", "CURSOR_SIZE", "DISABLE_CURSOR"}));
        CHECK_EQ(all[20].getName(), "Input");
        CHECK(names(all[21].getOptions()) == (std::vector<std::string>{"MULTITOUCH_GAMEPLAY", "TOUCH_MAX_FINGERS", "TOUCH_PALM_REJECTION"}));
        CHECK(names(all[23].getOptions()) == (std::vector<std::string>{"KEY_LEFT", "KEY_RIGHT"}));
        CHECK_EQ(all[24].getName(), "Custom");
        CHECK(names(all[25].getOptions()) == (std::vector<std::string>{"FIXED_CS", "FIXED_HP", "FIXED_AR", "FIXED_OD", "FIXED_SPEED"}));
        CHECK(names(all[26].getOptions()) == (std::vector<std::string>{"CHECKPOINT", "REPLAY_SEEKING"}));
        all[1].setVisible(false);
        CHECK(!OptionGroup::allOptions()[1].isVisible());   // flags persist in the cached list
    }
    // desktop: same shape, the two desktop-only options appear
    {
        OptionsSeams s;
        s.isDesktop = [] { return true; };
        Options::setSeams(s);
        OptionGroup::rebuild();
        auto& all = OptionGroup::allOptions();
        CHECK(all.size() == 27);
        CHECK(names(all[4].getOptions()) == (std::vector<std::string>{"SCREEN_RESOLUTION", "MOBILE_UI_SCALING"}));
        CHECK(names(all[5].getOptions()) == (std::vector<std::string>{"TARGET_FPS", "SHOW_FPS"}));
        CHECK(OptionGroup::allOptions()[1].isVisible());    // rebuilt
    }
    // loader quick settings: 3 sections + 3 groups
    {
        auto& l = OptionGroup::loaderOptions();
        CHECK(l.size() == 6);
        CHECK_EQ(l[0].getName(), "Gameplay");
        CHECK_EQ(l[1].getName(), "VISUAL SETTINGS");
        CHECK(names(l[1].getOptions()) ==
              (std::vector<std::string>{"BACKGROUND_DIM", "BACKGROUND_BLUR", "ENABLE_VIDEOS", "ENABLE_STORYBOARD", "IGNORE_BEATMAP_SKINS"}));
        CHECK_EQ(l[2].getName(), "Audio");
        CHECK(names(l[3].getOptions()) ==
              (std::vector<std::string>{"MUSIC_VOLUME", "HITSOUND_VOLUME", "DISABLE_GAMEPLAY_SOUNDS", "MUSIC_OFFSET"}));
        CHECK_EQ(l[4].getName(), "Input");
        CHECK(names(l[5].getOptions()) == std::vector<std::string>{"DISABLE_MOUSE_BUTTONS"});
    }
    Options::resetForTests();
    OptionGroup::rebuild();
}

static void testMultitouchSeam() {
    Options::resetForTests();
    CHECK(!Options::isMultitouchGameplay());              // desktop / no seam: false even though the option is on
    OptionsSeams s;
    s.isTouchPlatform = [] { return true; };
    Options::setSeams(s);
    CHECK(Options::isMultitouchGameplay());
    GameOption::MULTITOUCH_GAMEPLAY.setValue(false);
    CHECK(!Options::isMultitouchGameplay());
}

static void testDirtyAndSortOrder() {
    Options::resetForTests();
    int64_t t = 1000;
    OptionsSeams s;
    s.nowMs = [&] { return t; };
    Options::setSeams(s);
    CHECK(!Options::isDirty());
    t = 5000;
    GameOption::MASTER_VOLUME.setValue(10);
    CHECK(Options::isDirty() && Options::getDirtySinceMs() == 5000);
    Options::resetForTests();
    CHECK(Options::getSavedSortOrder().empty());
    Options::setSavedSortOrder("");                        // null in Java: ignored
    CHECK(!Options::isDirty());
    Options::setSavedSortOrder("TITLE");
    CHECK_EQ(Options::getSavedSortOrder(), "TITLE");
    CHECK(Options::isDirty());
    Options::resetForTests();
    Options::setSavedSortOrder("TITLE");
    Options::resetForTests();
    CHECK(Options::getSavedSortOrder().empty());
}

static void testConcurrency() {
    Options::resetForTests();
    std::atomic<bool> stop{false};
    std::atomic<long> reads{0}, bad{0};
    auto reader = [&] {
        while (!stop.load()) {
            float m = Options::getMasterVolume();
            if (m < 0.0f || m > 1.0f) bad++;
            int off = Options::getMusicOffset();
            if (off < -500 || off > 500) bad++;
            (void)Options::getGameKeyLeft();
            (void)Options::getSkinName();
            (void)GameOption::MASTER_VOLUME.getValueString();
            (void)Options::isMultitouchGameplay();
            reads++;
        }
    };
    std::thread r1(reader), r2(reader);
    std::thread w([&] {
        for (int i = 0; i < 20000; i++) {
            GameOption::MASTER_VOLUME.setValue(i % 101);
            GameOption::MUSIC_OFFSET.setValue((i % 1001) - 500);
            Options::setSkinName(i % 2 ? "A" : "B");
            Options::setGameKeyLeft(29 + i % 20);
            GameOption::SHOW_FPS.toggle(nullptr);
            GameOption::MUSIC_VOLUME.read(std::to_string(i % 101));
        }
    });
    w.join();
    stop = true;
    r1.join();
    r2.join();
    CHECK(bad.load() == 0);
    CHECK(reads.load() > 0);
}


// =====================================================================================================================
// Milestone 2: init / paths / directories / opsu.cfg
// =====================================================================================================================
namespace fs = std::filesystem;

struct TmpDir {
    std::string path;
    TmpDir() {
        static int counter = 0;
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        fs::path p = fs::temp_directory_path() / ("opsu_t10_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        fs::create_directories(p);
        path = fs::canonical(p).generic_string();
    }
    ~TmpDir() { std::error_code ec; fs::remove_all(path, ec); }
    std::string sub(const std::string& n) const { return path + "/" + n; }
};

static OptionsPaths pathsIn(const TmpDir& t) {
    OptionsPaths p;
    p.configDir = t.sub("cfg");
    p.dataDir = t.sub("data");
    p.cacheDir = t.sub("cache");
    p.localDir = t.sub("local");
    p.downloadDir = t.sub("dl");
    return p;
}
static bool exists(const std::string& p) { std::error_code ec; return fs::exists(p, ec); }
static bool isDirectory(const std::string& p) { std::error_code ec; return fs::is_directory(p, ec); }
static std::string slurp(const std::string& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
static void spit(const std::string& p, const std::string& text) {
    fs::create_directories(fs::path(p).parent_path());
    std::ofstream out(p, std::ios::binary);
    out << text;
}
static std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> v;
    size_t i = 0;
    while (i < text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos) { v.push_back(text.substr(i)); break; }
        v.push_back(text.substr(i, j - i));
        i = j + 1;
    }
    return v;
}

static void testPathsAndNoIoBeforeInit() {
    TmpDir t;
    Options::resetForTests();
    // before init(): no paths, and nothing is read or written
    CHECK(!Options::isInitialized());
    CHECK(Options::getBeatmapDir().empty() && Options::getOptionsFile().empty() && Options::getSkinRootDir().empty());
    CHECK(Options::getSkinDir().empty() && Options::getImportDir().empty() && Options::getLogFile().empty());
    GameOption::MASTER_VOLUME.setValue(12);           // dirty
    Options::saveOptions();
    Options::flushNow();
    Options::parseOptions();
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 12);
    CHECK(!Options::isDirty());                       // saveOptions() clears the flag even when it cannot write (Java order)
    // the default OptionsPaths point into the system temp directory
    OptionsPaths def;
    CHECK(def.configDir == def.dataDir && def.dataDir == def.cacheDir);
    CHECK(def.dataDir.find("opsu-nx-options") != std::string::npos);

    Options::resetForTests();
    OptionsPaths p = pathsIn(t);
    Options::init(p);
    CHECK(Options::isInitialized());
    CHECK(!exists(p.configDir) && !exists(p.dataDir) && !exists(p.cacheDir));   // init() does no I/O
    // derived names (Options.java: LOG_FILE, OPTIONS_FILE, O_OPTIONS_FILE, BEATMAP_DB, ... NATIVE_DIR, TEMP_DIR)
    CHECK_EQ(Options::getConfigDir(), t.sub("cfg"));
    CHECK_EQ(Options::getDataDir(), t.sub("data"));
    CHECK_EQ(Options::getCacheDir(), t.sub("cache"));
    CHECK_EQ(Options::getLogFile(), t.sub("cfg/opsu.log"));
    CHECK_EQ(Options::getOptionsFile(), t.sub("cfg/opsu.cfg"));
    CHECK_EQ(Options::getOldOptionsFile(), t.sub(".opsu.cfg"));     // new File(CONFIG_DIR, "../.opsu.cfg")
    CHECK_EQ(Options::getBeatmapDB(), t.sub("data/opsu.db"));
    CHECK_EQ(Options::getOldBeatmapDB(), t.sub("data/.opsu.db"));
    CHECK_EQ(Options::getLocalBeatmapDB(), t.sub("local/opsu.db"));
    CHECK_EQ(Options::getScoreDB(), t.sub("data/opsu_scores.db"));
    CHECK_EQ(Options::getOldScoreDB(), t.sub("data/.opsu_scores.db"));
    CHECK_EQ(Options::getLocalScoreDB(), t.sub("local/opsu_scores.db"));
    CHECK_EQ(Options::getNativeDir(), t.sub("cache/Natives"));
    CHECK_EQ(Options::getTempDir(), t.sub("cache/Temp"));
    CHECK(!exists(p.configDir) && !exists(p.dataDir) && !exists(p.cacheDir));   // the name getters never touch the disk

    // beatmap / import / skins are created on first use; screenshots / replays only get a name
    CHECK_EQ(Options::getBeatmapDir(), t.sub("data/Songs"));
    CHECK(isDirectory(t.sub("data/Songs")));
    CHECK_EQ(Options::getImportDir(), t.sub("data/Import"));
    CHECK(isDirectory(t.sub("data/Import")));
    CHECK_EQ(Options::getSkinRootDir(), t.sub("data/Skins"));
    CHECK(isDirectory(t.sub("data/Skins")));
    CHECK_EQ(Options::getScreenshotDir(), t.sub("data/Screenshots"));
    CHECK_EQ(Options::getReplayDir(), t.sub("data/Replays"));
    CHECK(!exists(t.sub("data/Screenshots")) && !exists(t.sub("data/Replays")));

    // a configured, valid directory wins (and is what gets written); an invalid one falls back to the default
    fs::create_directories(t.sub("mysongs"));
    GameOption::BEATMAP_DIRECTORY.read(t.sub("mysongs"));
    CHECK_EQ(Options::getBeatmapDir(), t.sub("mysongs"));
    CHECK_EQ(GameOption::BEATMAP_DIRECTORY.write(), t.sub("mysongs"));
    GameOption::BEATMAP_DIRECTORY.read(t.sub("gone"));
    CHECK_EQ(Options::getBeatmapDir(), t.sub("data/Songs"));
    GameOption::SCREENSHOT_DIRECTORY.read(t.sub("mysongs"));          // valid -> used
    CHECK_EQ(Options::getScreenshotDir(), t.sub("mysongs"));
    GameOption::REPLAY_DIRECTORY.read(t.sub("gone"));                 // invalid -> default name, not created
    CHECK_EQ(Options::getReplayDir(), t.sub("data/Replays"));
    CHECK(!exists(t.sub("data/Replays")));

    // skin directory: <root>/<skinName> if it exists, else "" (default skin)
    CHECK(Options::getSkinDir().empty());
    fs::create_directories(t.sub("data/Skins/MySkin"));
    Options::setSkinName("MySkin");
    CHECK_EQ(Options::getSkinDir(), t.sub("data/Skins/MySkin"));
    Options::setSkinName("Nope");
    CHECK(Options::getSkinDir().empty());
    // SKIN item list: "Default" + SkinLoader.getSkinDirectories(root) (seam), selectItem asks SkinSwitcher
    std::string seenRoot, requested;
    OptionsSeams sm;
    sm.listSkinDirectories = [&](const std::string& root) { seenRoot = root; return std::vector<std::string>{"a", "b"}; };
    sm.requestSkin = [&](const std::string& n) { requested = n; };
    Options::setSeams(sm);
    auto items = GameOption::SKIN.getItemList();
    CHECK(items == (std::vector<std::string>{"Default", "a", "b"}) && seenRoot == t.sub("data/Skins"));
    GameOption::SKIN.selectItem(2);
    CHECK_EQ(requested, "b");
    requested.clear();
    GameOption::SKIN.selectItem(3);                                   // out of range: ignored
    CHECK(requested.empty());

    // FFmpeg: "" = default
    CHECK(Options::getFFmpegLocation().empty());
    GameOption::FFMPEG_PATH.read("/opt/ffmpeg/libffmpeg.so");
    CHECK_EQ(Options::getFFmpegLocation(), "/opt/ffmpeg/libffmpeg.so");
    CHECK_EQ(GameOption::FFMPEG_PATH.write(), "/opt/ffmpeg/libffmpeg.so");

    // download dir: the platform folder if it exists, else (desktop) ~/Downloads, else the relative "Download"
    CHECK_EQ(Options::getDownloadDir(), "Download");
    fs::create_directories(t.sub("dl"));
    CHECK_EQ(Options::getDownloadDir(), t.sub("dl"));
    fs::remove_all(t.sub("dl"));
#ifndef _WIN32
    fs::create_directories(t.sub("home/Downloads"));
    setenv("HOME", t.sub("home").c_str(), 1);
    CHECK_EQ(Options::getDownloadDir(), "Download");                  // isDesktop seam is false
    OptionsSeams sd;
    sd.isDesktop = [] { return true; };
    Options::setSeams(sd);
    CHECK_EQ(Options::getDownloadDir(), t.sub("home/Downloads"));
#endif
    // init() forgets directories read earlier
    GameOption::BEATMAP_DIRECTORY.read(t.sub("mysongs"));
    Options::init(p);
    CHECK_EQ(Options::getBeatmapDir(), t.sub("data/Songs"));
}

// a fixture with every kind of line the parser meets; CRLF line endings
static void testConfigParsing() {
    TmpDir t;
    Options::resetForTests();
    Options::init(pathsIn(t));
    const std::string cfg = t.sub("cfg/opsu.cfg");
    std::string text =
        "# opsu! configuration\r\n"
        "# last updated on Monday, January 01, 2024\r\n"
        "\r\n"
        "SliderStyleVersion = 1\r\n"
        "SongSortOrder = ARTIST\r\n"
        "VolumeUniversal = 80\r\n"                // numeric, in range
        "VolumeMusic=55\r\n"                      // no spaces around '='
        "   VolumeEffect   =   20   \r\n"         // blanks everywhere: trimmed
        "VolumeHitSound = 150\r\n"                // out of range: ignored, stays 30
        "Offset = abc\r\n"                        // malformed number: logged and skipped
        "RenderOffset = 12.5\r\n"                 // Integer.parseInt rejects decimals
        "CursorSize = 1.5\r\n"
        "FixedCS = 4.5\r\n"
        "Fullscreen = true\r\n"                   // boolean
        "SnakingSliders = false\r\n"              // version 1 present: no migration, stays false
        "FrameSync = 240\r\n"
        "ScreenshotFormat = 2\r\n"
        "ScreenResolution = 1280x720\r\n"
        "Skin = My Skin = x\r\n"                  // only the FIRST '=' splits
        "keyOsuLeft = A\r\n"
        "keyOsuRight = Q\r\n"
        "UnknownKey = 5\r\n"                      // unknown keys are ignored
        "=5\r\n"                                  // empty name: ignored
        "x\r\n"                                   // shorter than 2 characters
        "just some words\r\n"                     // no '='
        "# Offset = 100\r\n"                      // comment
        "  # indented = 3  \r\n"                  // comment after trim
        "ThemeSongTiming = 0,500.0,4,1,0,100,0,0\r\n"
        "MouseDisableWheel = TRUE\r\n"            // Boolean.parseBoolean ignores case
        "DisableCursor = 1\r\n"                   // anything but "true" is false
        "ExperimentalSliderCaps = true";          // last line without a terminator
    spit(cfg, text);
    std::string before = text;
    Options::parseOptions();
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 80);
    CHECK(GameOption::MUSIC_VOLUME.getIntegerValue() == 55);
    CHECK(GameOption::EFFECT_VOLUME.getIntegerValue() == 20);
    CHECK(GameOption::HITSOUND_VOLUME.getIntegerValue() == 30);
    CHECK(Options::getMusicOffset() == 0 && Options::getRenderOffset() == 0);
    CHECK(Options::getCursorScale() == 1.5f);
    CHECK(Options::getFixedCS() == 4.5f);
    CHECK(Options::isFullscreen());                      // read() assigns directly (toggle's checks do not apply)
    CHECK(!Options::isSliderSnaking());
    CHECK(Options::getTargetFPS() == 240);
    CHECK_EQ(Options::getScreenshotFormat(), "bmp");
    CHECK_EQ(GameOption::SCREEN_RESOLUTION.getValueString(), "1280x720");
    CHECK_EQ(Options::getSkinName(), "My Skin = x");
    CHECK(Options::getGameKeyLeft() == 29 && Options::getGameKeyRight() == 45);
    CHECK_EQ(Options::getSavedSortOrder(), "ARTIST");
    CHECK_EQ(GameOption::THEME_SONG_TIMINGPOINT.write(), "0,500.0,4,1,0,100,0,0");
    CHECK(Options::isMouseWheelDisabled() && !Options::isCursorDisabled());
    CHECK(Options::isSliderCapsDrawn());
    CHECK(Options::getSliderStyleVersion() == 1);
    CHECK(!Options::isDirty());
    CHECK_EQ(slurp(cfg), before);                        // version 1: the file is not rewritten

    // bare '\r' line endings and no final newline
    Options::resetForTests();
    Options::init(pathsIn(t));
    spit(cfg, "SliderStyleVersion = 1\rVolumeMusic = 10\rVolumeEffect = 11");
    Options::parseOptions();
    CHECK(GameOption::MUSIC_VOLUME.getIntegerValue() == 10 && GameOption::EFFECT_VOLUME.getIntegerValue() == 11);

    // a bad SliderStyleVersion counts as 0 -> migration
    Options::resetForTests();
    Options::init(pathsIn(t));
    spit(cfg, "SliderStyleVersion = x\nSnakingSliders = false\n");
    Options::parseOptions();
    CHECK(Options::isSliderSnaking() && Options::getSliderStyleVersion() == 1);

    // an old file (no SliderStyleVersion): sliders reset once to the single-style defaults, then the file is rewritten
    Options::resetForTests();
    Options::init(pathsIn(t));
    spit(cfg, "SnakingSliders = false\nExperimentalSliderCaps = true\nExperimentalSliderShrink = false\nExperimentalSliderMerge = false\nVolumeMusic = 12\n");
    Options::parseOptions();
    CHECK(Options::isSliderSnaking() && Options::isSliderShrinking() && Options::isSliderMerging() && !Options::isSliderCapsDrawn());
    CHECK(GameOption::MUSIC_VOLUME.getIntegerValue() == 12);
    CHECK(Options::getSliderStyleVersion() == 1 && !Options::isDirty());
    std::string rewritten = slurp(cfg);
    CHECK(rewritten.find("SliderStyleVersion = 1\n") != std::string::npos);
    CHECK(rewritten.find("VolumeMusic = 12\n") != std::string::npos);
    CHECK(rewritten.find("ExperimentalSliderCaps = false\n") != std::string::npos);

    // missing file: the defaults are written
    Options::resetForTests();
    Options::init(pathsIn(t));
    fs::remove(cfg);
    Options::parseOptions();
    CHECK(exists(cfg));
    CHECK(linesOf(slurp(cfg)).size() == 78);             // 3 header + SliderStyleVersion + 74 options

    // UTF-8 text survives; invalid bytes become U+FFFD (EF BF BD) like the JDK decoder
    Options::resetForTests();
    Options::init(pathsIn(t));
    spit(cfg, "SliderStyleVersion = 1\nSkin = Sk\xC3\xAFn \xE2\x9C\x93\n");
    Options::parseOptions();
    CHECK_EQ(Options::getSkinName(), "Sk\xC3\xAFn \xE2\x9C\x93");
    spit(cfg, "SliderStyleVersion = 1\nSkin = bad\xFF\n");
    Options::parseOptions();
    CHECK_EQ(Options::getSkinName(), "bad\xEF\xBF\xBD");
}

static void testWriteFormat() {
    TmpDir t;
    Options::resetForTests();
#ifndef _WIN32
    setenv("TZ", "UTC", 1);
    tzset();
#endif
    OptionsSeams sm;
    sm.nowMs = [] { return int64_t(1700000000000LL); };   // Tuesday, 14 November 2023 22:13:20 UTC
    Options::setSeams(sm);
    OptionsPaths p = pathsIn(t);
    Options::init(p);
    GameOption::SLIDER_CAPS.setValue(false);
    Options::saveOptions();
    std::string text = slurp(t.sub("cfg/opsu.cfg"));
    CHECK(!text.empty() && text.back() == '\n');
    CHECK(text.find('\r') == std::string::npos);                       // "\n" line endings
    CHECK(!(text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF));   // no BOM
    CHECK(!exists(t.sub("cfg/opsu.cfg.tmp")));                         // temp file renamed away
    auto lines = linesOf(text);
    CHECK(lines.size() == 78);
    CHECK_EQ(lines[0], "# opsu! configuration");
#ifndef _WIN32
    CHECK_EQ(lines[1], "# last updated on Tuesday, November 14, 2023");  // "EEEE, MMMM dd, yyyy"
#endif
    CHECK_EQ(lines[2], "");
    CHECK_EQ(lines[3], "SliderStyleVersion = 1");
    // every option in Java order, "<DisplayName> = <write()>", NULL skipped
    int li = 4;
    for (int i = 0; i < kRowCount; i++) {
        if (kRows[i].id == Id::NULL_OPTION) continue;
        CHECK(li < static_cast<int>(lines.size()) && lines[li].rfind(std::string(kRows[i].key) + " = ", 0) == 0);
        li++;
    }
    std::map<std::string, std::string> kv;
    for (size_t i = 4; i < lines.size(); i++) {
        size_t e = lines[i].find(" = ");
        kv[lines[i].substr(0, e)] = lines[i].substr(e + 3);
    }
    // by hand from the write() overrides in Options.java
    CHECK_EQ(kv["BeatmapDirectory"], t.sub("data/Songs"));
    CHECK_EQ(kv["ImportDirectory"], t.sub("data/Import"));
    CHECK_EQ(kv["ScreenshotDirectory"], t.sub("data/Screenshots"));
    CHECK_EQ(kv["ReplayDirectory"], t.sub("data/Replays"));
    CHECK_EQ(kv["SkinDirectory"], t.sub("data/Skins"));
    CHECK_EQ(kv["FFmpegPath"], "");                                    // "FFmpegPath = " (trailing space kept)
    CHECK(lines[4 + 5] == "FFmpegPath = ");
    CHECK_EQ(kv["ThemeSong"], "theme.mp3,Rainbows,Kevin MacLeod,219350");
    CHECK_EQ(kv["ThemeSongTiming"], "1120,545.454545454545,4,1,0,100,0,0");
    CHECK_EQ(kv["ScreenResolution"], "1024x768");
    CHECK_EQ(kv["Fullscreen"], "false");
    CHECK_EQ(kv["Skin"], "Default");
    CHECK_EQ(kv["FrameSync"], "60");
    CHECK_EQ(kv["FpsCounter"], "true");
    CHECK_EQ(kv["ScreenshotFormat"], "0");
    CHECK_EQ(kv["CursorSize"], "1.00");
    CHECK_EQ(kv["VolumeUniversal"], "35");
    CHECK_EQ(kv["VolumeMusic"], "80");
    CHECK_EQ(kv["Offset"], "0");
    CHECK_EQ(kv["keyOsuLeft"], "Z");
    CHECK_EQ(kv["keyOsuRight"], "X");
    CHECK_EQ(kv["MultitouchGameplay"], "true");
    CHECK_EQ(kv["TouchMaxFingers"], "10");
    CHECK_EQ(kv["ExperimentalSliderCaps"], "false");
    CHECK_EQ(kv["FixedCS"], "0.0");
    CHECK_EQ(kv["FixedSpeed"], "0.00");
    CHECK_EQ(kv["Checkpoint"], "0");
    CHECK_EQ(kv["MobileUIScale"], "1.0");
    CHECK_EQ(kv["PlayfieldScale"], "1.0");
    CHECK(kv.count("SongSortOrder") == 0);
    // writing the directory options created the ones the Java creates
    CHECK(isDirectory(t.sub("data/Songs")) && isDirectory(t.sub("data/Import")) && isDirectory(t.sub("data/Skins")));
    CHECK(!exists(t.sub("data/Screenshots")));
    // with a saved sort order it comes right after SliderStyleVersion
    Options::setSavedSortOrder("TITLE");
    Options::saveOptions();
    lines = linesOf(slurp(t.sub("cfg/opsu.cfg")));
    CHECK(lines.size() == 79 && lines[3] == "SliderStyleVersion = 1" && lines[4] == "SongSortOrder = TITLE");
}

static std::vector<std::string> snapshot() {
    std::vector<std::string> v;
    for (const GameOption& o : GameOption::values()) {
        v.push_back(std::string(o.constantName()) + "|" + o.write() + "|" + o.getValueString() + "|" +
                    std::to_string(o.getIntegerValue()) + "|" + (o.getBooleanValue() ? "1" : "0"));
    }
    v.push_back("fps" + std::to_string(Options::getTargetFPS()));
    v.push_back("ss" + Options::getScreenshotFormat());
    v.push_back("skin" + Options::getSkinName());
    v.push_back("keys" + std::to_string(Options::getGameKeyLeft()) + "," + std::to_string(Options::getGameKeyRight()));
    v.push_back("sort" + Options::getSavedSortOrder());
    return v;
}

static void testRoundTrip() {
    TmpDir t;
    OptionsPaths p = pathsIn(t);
    Options::resetForTests();
    Options::init(p);
    // change a bit of everything
    GameOption::MASTER_VOLUME.setValue(61);
    GameOption::MUSIC_VOLUME.setValue(7);
    GameOption::MUSIC_OFFSET.setValue(-123);
    GameOption::CURSOR_SIZE.setValue(175);
    GameOption::FIXED_AR.setValue(75);
    GameOption::FIXED_SPEED.setValue(125);
    GameOption::CHECKPOINT.setValue(95);
    GameOption::MOBILE_UI_SCALING.setValue(15);
    GameOption::PLAYFIELDSCALE.setValue(17);
    GameOption::TARGET_FPS.selectItem(2);
    GameOption::SCREENSHOT_FORMAT.selectItem(1);
    GameOption::SCREEN_RESOLUTION.read("1280x720");
    GameOption::SHOW_INTRO.setValue(false);
    GameOption::SLIDER_CAPS.setValue(true);
    GameOption::SNAKING_SLIDERS.setValue(false);
    GameOption::DISABLE_SOUNDS.setValue(true);
    GameOption::THEME_SONG_TIMINGPOINT.read("5,400.5,4,1,0,100,0,0");
    Options::setSkinName("Sk\xC3\xAFn \xE2\x9C\x93");
    Options::setSavedSortOrder("DATE");
    Options::setGameKeyLeft(30);
    Options::setGameKeyRight(31);
    GameOption::FFMPEG_PATH.read("/opt/ff.so");
    fs::create_directories(t.sub("custom/replays"));
    GameOption::REPLAY_DIRECTORY.read(t.sub("custom/replays"));
    auto before = snapshot();
    Options::saveOptions();

    // fresh process: reset, init, parse
    Options::resetForTests();
    Options::init(p);
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 35);          // really back to defaults
    Options::parseOptions();
    auto after = snapshot();
    CHECK(before.size() == after.size());
    for (size_t i = 0; i < before.size() && i < after.size(); i++) {
        if (before[i] != after[i]) { std::printf("   round trip differs: %s  !=  %s\n", before[i].c_str(), after[i].c_str()); }
        CHECK(before[i] == after[i]);
    }
    CHECK_EQ(Options::getReplayDir(), t.sub("custom/replays"));
    CHECK(Options::getTargetFPS() == 240 && GameOption::CURSOR_SIZE.getIntegerValue() == 175 && Options::getMusicOffset() == -123);
    // and writing again gives the identical file apart from the date line
    std::string a = slurp(t.sub("cfg/opsu.cfg"));
    Options::saveOptions();
    std::string b = slurp(t.sub("cfg/opsu.cfg"));
    auto la = linesOf(a), lb = linesOf(b);
    CHECK(la.size() == lb.size());
    for (size_t i = 0; i < la.size() && i < lb.size(); i++) if (i != 1) CHECK(la[i] == lb[i]);
}

static void testFlushDebounce() {
    TmpDir t;
    Options::resetForTests();
    int64_t now = 1000;
    OptionsSeams sm;
    sm.nowMs = [&] { return now; };
    Options::setSeams(sm);
    Options::init(pathsIn(t));
    const std::string cfg = t.sub("cfg/opsu.cfg");
    Options::flushIfDue();
    Options::flushNow();
    CHECK(!exists(cfg));                          // nothing dirty: nothing written
    GameOption::MASTER_VOLUME.setValue(44);       // dirtySince = 1000
    now = 1399;
    Options::flushIfDue();
    CHECK(!exists(cfg) && Options::isDirty());    // 399 ms: still debouncing (SAVE_DEBOUNCE_MS = 400)
    GameOption::MASTER_VOLUME.setValue(45);       // another change restarts the timer at 1399
    now = 1798;
    Options::flushIfDue();
    CHECK(!exists(cfg));
    now = 1799;
    Options::flushIfDue();
    CHECK(exists(cfg) && !Options::isDirty());
    CHECK(slurp(cfg).find("VolumeUniversal = 45\n") != std::string::npos);
    fs::remove(cfg);
    Options::flushNow();                          // not dirty: no write
    CHECK(!exists(cfg));
    GameOption::MASTER_VOLUME.setValue(46);
    Options::flushNow();                          // dirty: written at once
    CHECK(exists(cfg) && !Options::isDirty());
    CHECK(slurp(cfg).find("VolumeUniversal = 46\n") != std::string::npos);
}

static void testConcurrencyWithSave() {
    TmpDir t;
    Options::resetForTests();
    Options::init(pathsIn(t));
    std::atomic<bool> stop{false};
    std::atomic<long> bad{0};
    std::thread reader([&] {
        while (!stop.load()) {
            float m = Options::getMasterVolume();
            if (m < 0.0f || m > 1.0f) bad++;
            if (Options::getBeatmapDir().empty() || Options::getOptionsFile().empty()) bad++;
            (void)Options::getSkinDir();
        }
    });
    std::thread saver([&] { for (int i = 0; i < 100; i++) Options::saveOptions(); });
    std::thread parser([&] { for (int i = 0; i < 50; i++) Options::parseOptions(); });
    std::thread setter([&] {
        for (int i = 0; i < 3000; i++) {
            GameOption::MASTER_VOLUME.setValue(i % 101);
            Options::setSkinName(i % 2 ? "A" : "B");
            Options::flushIfDue();
        }
    });
    setter.join(); saver.join(); parser.join();
    stop = true;
    reader.join();
    CHECK(bad.load() == 0);
    Options::saveOptions();
    CHECK(linesOf(slurp(t.sub("cfg/opsu.cfg"))).size() == 78);
    CHECK(!exists(t.sub("cfg/opsu.cfg.tmp")));
}

// =====================================================================================================================
// Milestone 3: wiring helpers
// =====================================================================================================================
static void testWiringHosts() {
    TmpDir t;
    Options::resetForTests();
    Options::init(pathsIn(t));
    GameOption::MASTER_VOLUME.setValue(50);
    GameOption::MUSIC_VOLUME.setValue(40);
    GameOption::EFFECT_VOLUME.setValue(20);
    GameOption::HITSOUND_VOLUME.setValue(10);
    GameOption::MUSIC_OFFSET.setValue(-7);

    MusicController::setHost(OptionsWiring::musicControllerHost());
    const auto& mh = MusicController::host();
    CHECK(mh.musicVolume() == 0.4f && mh.masterVolume() == 0.5f && mh.musicOffset() == -7);
    CHECK(mh.themeBeatmap() == nullptr);                  // seam kept: no theme beatmap
    GameOption::MUSIC_OFFSET.setValue(12);                // read live, not copied
    CHECK(MusicController::host().musicOffset() == 12);
    static const Beatmap theme("theme.osu");
    MusicController::setHost(OptionsWiring::musicControllerHost([] { return &theme; }));
    CHECK(MusicController::host().themeBeatmap() == &theme);
    MusicController::setHost(MusicController::Host());     // restore defaults for other tests

    fs::create_directories(t.sub("data/Skins/S1"));
    Options::setSkinName("S1");
    SoundController::setHost(OptionsWiring::soundControllerHost());
    const auto& sh = SoundController::host();
    CHECK(sh.effectVolume() == 0.2f && sh.hitSoundVolume() == 0.1f && sh.masterVolume() == 0.5f && sh.musicVolume() == 0.4f);
    CHECK(!sh.soundDisabled() && !sh.gameplaySoundDisabled());
    GameOption::DISABLE_SOUNDS.setValue(true);
    GameOption::DISABLE_GAMEPLAY_SOUNDS.setValue(true);
    CHECK(sh.soundDisabled() && sh.gameplaySoundDisabled());
    CHECK_EQ(sh.skinDirectory(), t.sub("data/Skins/S1"));
    Options::setSkinName("missing");
    CHECK(sh.skinDirectory().empty());                    // default skin: no directory (Java null)
    CHECK(sh.layeredHitSounds());                         // untouched default (a skin.ini setting)
    SoundController::setHost(SoundController::Host());

    // the volume seams reach MusicController (no player: must be harmless) and are installed on the struct we pass
    OptionsSeams seams;
    OptionsWiring::wireAudioSeams(seams);
    CHECK(static_cast<bool>(seams.onVolumeAdjusted) && static_cast<bool>(seams.setMusicControllerVolume));
    Options::setSeams(seams);
    MusicController::setHost(OptionsWiring::musicControllerHost());
    GameOption::MASTER_VOLUME.setValue(30);
    Options::setMasterVolume(nullptr, 0.6f);
    CHECK(GameOption::MASTER_VOLUME.getIntegerValue() == 60);
    MusicController::setHost(MusicController::Host());
}

static void testWiringImporter() {
    TmpDir t;
    Options::resetForTests();
    Options::init(pathsIn(t));
    std::vector<std::pair<std::string, std::string>> skins, maps;
    std::mutex mu;
    OptionsWiring::OptionsDownloadImporterEnv env(
        [&](const std::string& a, const std::string& b) { std::lock_guard<std::mutex> lk(mu); skins.emplace_back(a, b); },
        [&](const std::string& a, const std::string& b) { std::lock_guard<std::mutex> lk(mu); maps.emplace_back(a, b); });
    CHECK_EQ(env.getBeatmapDir(), t.sub("data/Songs"));
    CHECK_EQ(env.getSkinRootDir(), t.sub("data/Skins"));
    CHECK_EQ(env.getDownloadDir(), "Download");           // no platform folder yet
    DownloadImporter::importAll(env);                      // "Download" does not exist (relative): returns at once
    CHECK(skins.empty() && maps.empty());
    fs::create_directories(t.sub("dl"));
    CHECK_EQ(env.getDownloadDir(), t.sub("dl"));
    DownloadImporter::importAll(env);
    CHECK(skins.size() == 1 && maps.size() == 1);
    CHECK(skins[0].first == t.sub("dl") && skins[0].second == t.sub("data/Skins"));
    CHECK(maps[0].first == t.sub("dl") && maps[0].second == t.sub("data/Songs"));
}

static void testWiringBeatmapAndHitObject() {
    Options::resetForTests();
    // Beatmap accessors
    Beatmap b("x.osu");
    b.title = "Title"; b.titleUnicode = "Titel-U"; b.artist = "Artist"; b.artistUnicode = "Artist-U";
    CHECK_EQ(OptionsWiring::title(b), "Title");
    CHECK_EQ(OptionsWiring::artist(b), "Artist");
    GameOption::SHOW_UNICODE.read("true");
    CHECK_EQ(OptionsWiring::title(b), "Titel-U");
    CHECK_EQ(OptionsWiring::artist(b), "Artist-U");
    CHECK_EQ(OptionsWiring::toString(b), b.toString(true));
    CHECK(OptionsWiring::useUnicodeMetadata());
    // ModParams use the fixed speed / CS options
    CHECK(OptionsWiring::currentModParams().clockRate == 1.0f);
    GameOption::FIXED_SPEED.setValue(125);
    GameOption::FIXED_CS.setValue(55);
    auto mp = OptionsWiring::currentModParams();
    auto ref = Beatmap::ModParams::current(1.25f, 5.5f);
    CHECK(mp.clockRate == 1.25f && mp.key == ref.key);
    // HitObject scale: PLAYFIELDSCALE / 10f (1.0 default, 1.6 on a small screen)
    OptionsSeams s;
    s.screenWidthInches = [] { return 5.0f; };
    Options::setSeams(s);
    CHECK(Options::getPlayfieldScale() == 1.6f);
    OptionsWiring::initHitObject(1280, 720);
    float xm = HitObject::getXMultiplier(), ym = HitObject::getYMultiplier();
    int xo = HitObject::getXOffset(), yo = HitObject::getYOffset();
    HitObject::init(1280, 720, 1.6f);
    CHECK(xm == HitObject::getXMultiplier() && ym == HitObject::getYMultiplier());
    CHECK(xo == HitObject::getXOffset() && yo == HitObject::getYOffset());
    GameOption::PLAYFIELDSCALE.setValue(10);
    OptionsWiring::initHitObject(1280, 720);
    CHECK(HitObject::getXMultiplier() != xm);              // a different scale gives a different layout
    HitObject::init(1280, 720, 1.0f);
    // import dir / theme song data
    TmpDir t;
    Options::init(pathsIn(t));
    CHECK_EQ(OptionsWiring::importDir(), t.sub("data/Import"));
    ThemeSong ts;
    CHECK(Options::getThemeSong(ts));
    CHECK(ts.audioFilename == "theme.mp3" && ts.title == "Rainbows" && ts.artist == "Kevin MacLeod" && ts.endTime == 219350);
    CHECK_EQ(ts.timingPoint, "1120,545.454545454545,4,1,0,100,0,0");
}

int main() {
    testEnumAndDefaults();
    testAccessorsDefaults();
    testSettersClamp();
    testToggleAndSelect();
    testResolutionList();
    testVolumeSeams();
    testScreenDependentDefaults();
    testKeys();
    testValueStringsAndWrite();
    testRead();
    testMatches();
    testGroups();
    testMultitouchSeam();
    testDirtyAndSortOrder();
    testConcurrency();
    testPathsAndNoIoBeforeInit();
    testConfigParsing();
    testWriteFormat();
    testRoundTrip();
    testFlushDebounce();
    testConcurrencyWithSave();
    testWiringHosts();
    testWiringImporter();
    testWiringBeatmapAndHitObject();
    Options::resetForTests();
    if (failures == 0) std::printf("test_options_t10: all OK\n");
    else std::printf("test_options_t10: %d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
