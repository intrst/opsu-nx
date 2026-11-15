// Port of itdelatrisu.opsu.options.Options (milestone 1). See docs/agents/T10.md.
#include "opsu/options/Options.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>

#include "opsu/OpsuConstants.h"
#include "opsu/Utils.h"
#include "opsu/beatmap/TimingPoint.h"
#include "opsu/logging/Log.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace opsu {

namespace fs = std::filesystem;

OptionsPaths::OptionsPaths() {
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec);
    if (ec) tmp = ".";
    fs::path base = tmp / "opsu-nx-options";
    configDir = dataDir = cacheDir = base.generic_string();   // the Java uses one folder for all three
    localDir = (base / "local").generic_string();
    downloadDir = (base / "Download").generic_string();
}

namespace {

using OptionType = GameOption::OptionType;
using Id = GameOption::Id;
constexpr int N = GameOption::COUNT;

// ---------------------------------------------------------------------------------------------------------------------
// Static description of every option, in Java enum order. Generated from Options.java (name, key, description, default,
// min, max); the two screen-size dependent defaults (MOBILE_UI_SCALING, PLAYFIELDSCALE) are 0 here and resolved in
// applyDefaults().
// ---------------------------------------------------------------------------------------------------------------------
struct Meta {
    std::string name, key, desc;
    OptionType type = OptionType::SELECT;
    bool defBool = false;
    int defVal = 0, min = 0, max = 0;
    bool restart = false;
};

Meta I(const char* key) {  // internal option: GameOption(String displayName)
    Meta m;
    m.key = key;
    return m;
}
Meta S(const char* name, const char* key, const char* desc, bool restart) {
    Meta m;
    m.name = name; m.key = key; m.desc = desc; m.restart = restart;
    return m;
}
Meta B(const char* name, const char* key, const char* desc, bool def, bool restart) {
    Meta m = S(name, key, desc, restart);
    m.type = OptionType::BOOLEAN;
    m.defBool = def;
    return m;
}
Meta Nu(const char* name, const char* key, const char* desc, int def, int min, int max, bool restart) {
    Meta m = S(name, key, desc, restart);
    m.type = OptionType::NUMERIC;
    m.defVal = def; m.min = min; m.max = max;
    return m;
}

const std::array<Meta, GameOption::COUNT>& metas() {
    static const std::array<Meta, GameOption::COUNT> t = {{
        S("", "", "", false),  // NULL_OPTION
        I("BeatmapDirectory"),  // BEATMAP_DIRECTORY
        I("ImportDirectory"),  // IMPORT_DIRECTORY
        I("ScreenshotDirectory"),  // SCREENSHOT_DIRECTORY
        I("ReplayDirectory"),  // REPLAY_DIRECTORY
        I("SkinDirectory"),  // SKIN_DIRECTORY
        I("FFmpegPath"),  // FFMPEG_PATH
        I("ThemeSong"),  // THEME_SONG
        I("ThemeSongTiming"),  // THEME_SONG_TIMINGPOINT
        S("Resolution", "ScreenResolution", "", true),  // SCREEN_RESOLUTION
        B("Fullscreen mode", "Fullscreen", "Switches to dedicated fullscreen mode.", false, true),  // FULLSCREEN
        S("Skin", "Skin", "", false),  // SKIN
        S("Frame limiter", "FrameSync", "Higher values may cause high CPU usage.", false),  // TARGET_FPS
        B("Show FPS counter", "FpsCounter", "Show a subtle FPS counter in the bottom right corner of the screen.", true, false),  // SHOW_FPS
        B("Prefer metadata in original language", "ShowUnicode", "Where available, song titles will be shown in their native language (and character-set).", false, false),  // SHOW_UNICODE
        S("Screenshot format", "ScreenshotFormat", "Press F12 to take a screenshot.", false),  // SCREENSHOT_FORMAT
        Nu("Cursor size", "CursorSize", "Change the cursor scale.", 100, 50, 200, false),  // CURSOR_SIZE
        B("Dynamic backgrounds", "DynamicBackground", "The current beatmap background will be used as the main menu background.", true, false),  // DYNAMIC_BACKGROUND
        B("Show intro", "ShowIntro", "Play the intro animation when the game starts.", true, false),  // SHOW_INTRO
        B("Detailed loading progress", "LoadVerbose", "Display more verbose loading progress in the splash screen.", false, false),  // LOAD_VERBOSE
        Nu("Master", "VolumeUniversal", "Global volume level.", 35, 0, 100, false),  // MASTER_VOLUME
        Nu("Music", "VolumeMusic", "Music volume.", 80, 0, 100, false),  // MUSIC_VOLUME
        Nu("Effects", "VolumeEffect", "Menu and game sound effects volume.", 70, 0, 100, false),  // EFFECT_VOLUME
        Nu("Hit sounds", "VolumeHitSound", "Hit sounds volume.", 30, 0, 100, false),  // HITSOUND_VOLUME
        Nu("Universal offset", "Offset", "Adjust this value if hit objects are out of sync.", 0, -500, 500, false),  // MUSIC_OFFSET
        Nu("Render offset", "RenderOffset", "Adjust this value if hit objects are visually out of sync.", 0, -500, 500, false),  // RENDER_OFFSET
        B("Disable sound effects in gameplay", "DisableGameplaySound", "Mute all sound effects during gameplay only.", false, false),  // DISABLE_GAMEPLAY_SOUNDS
        B("Disable all sound effects", "DisableSound", "May resolve Linux sound driver issues.\nRequires a restart.", false, true),  // DISABLE_SOUNDS
        S("Left game key", "keyOsuLeft", "Select this option to input a key.", false),  // KEY_LEFT
        S("Right game key", "keyOsuRight", "Select this option to input a key.", false),  // KEY_RIGHT
        B("Disable mouse wheel in play mode", "MouseDisableWheel", "During play, you can use the mouse wheel to adjust the volume and pause the game.\nThis will disable that functionality.", false, false),  // DISABLE_MOUSE_WHEEL
        B("Disable touch/mouse buttons in play mode", "MouseDisableButtons", "This option will disable all touch press/mouse buttons.\nSpecifically for people who use their keyboard to click.", false, false),  // DISABLE_MOUSE_BUTTONS
        B("Multitouch gameplay input", "MultitouchGameplay", "Every finger taps and tracks independently during play, like osu!droid.\nMenus keep the normal single-cursor touch input.\nOnly applies on touch devices.", true, false),  // MULTITOUCH_GAMEPLAY
        Nu("Max simultaneous fingers", "TouchMaxFingers", "Maximum number of fingers that can be down at once during play.\nWhen exceeded, the oldest finger is dropped (osu!droid uses 3).", 10, 1, 10, false),  // TOUCH_MAX_FINGERS
        B("Palm rejection", "TouchPalmRejection", "Ignores touches that land well outside the playfield (palm or thumb\nresting on a corner/edge) so they never count as a finger.\nButtons (pause, skip) still work.", true, false),  // TOUCH_PALM_REJECTION
        B("Disable cursor", "DisableCursor", "Hides the cursor sprite.", false, false),  // DISABLE_CURSOR
        Nu("Background dim", "DimLevel", "Percentage to dim the background image during gameplay.", 50, 0, 100, false),  // BACKGROUND_DIM
        Nu("Background blur", "BlurLevel", "Blurs the beatmap background during gameplay and on the loading screen.\nLightweight (no shaders), so it is fine on low-end and Android devices.", 0, 0, 100, false),  // BACKGROUND_BLUR
        B("Force default playfield", "ForceDefaultPlayfield", "Overrides the song background with the default playfield background.", false, false),  // FORCE_DEFAULT_PLAYFIELD
        B("Storyboard", "Storyboard", "Displays beatmap storyboards (.osb and [Events] sprites, animations and samples) during gameplay,\nlike osu!lazer. The storyboard follows the background dim setting.", true, false),  // ENABLE_STORYBOARD
        B("Background video", "Video", "Enables background video playback.\nIf you get a large amount of lag on beatmaps with video, try disabling this feature.", true, false),  // ENABLE_VIDEOS
        B("Ignore all beatmap skins", "IgnoreBeatmapSkins", "Defaults game settings to never use skin element overrides provided by beatmaps.", false, false),  // IGNORE_BEATMAP_SKINS
        B("Always use skin cursor", "UseSkinCursor", "The selected skin's cursor will override any beatmap-specific cursor modifications.", false, false),  // FORCE_SKIN_CURSOR
        B("Snaking sliders", "SnakingSliders", "Sliders gradually snake out from their starting point.", true, false),  // SNAKING_SLIDERS
        B("Draw slider caps", "ExperimentalSliderCaps", "Draw caps (end circles) on sliders.", false, false),  // SLIDER_CAPS
        B("Shrinking sliders", "ExperimentalSliderShrink", "Sliders shrink toward their ending point when the ball passes.", true, false),  // SLIDER_SHRINK
        B("Merging sliders", "ExperimentalSliderMerge", "For overlapping sliders, don't draw the edges and combine the slider tracks where they cross.", true, false),  // SLIDER_MERGE
        B("Hit lighting", "HitLighting", "Adds a subtle glow behind hit explosions which lights the playfield.", true, false),  // SHOW_HIT_LIGHTING
        B("Hit stars", "HitStars", "Default skin only: stars burst out of a circle when you hit it, like the triangles of osu!lazer.", true, false),  // SHOW_HIT_STARS
        B("Combo bursts", "ComboBurst", "A character image bursts from the side of the screen at combo milestones.", true, false),  // SHOW_COMBO_BURSTS
        B("Perfect hits", "PerfectHit", "Shows perfect hit result bursts (300s, slider ticks).", true, false),  // SHOW_PERFECT_HIT
        B("Follow points", "FollowPoints", "Shows follow points between hit objects.", true, false),  // SHOW_FOLLOW_POINTS
        B("Hit error bar", "ScoreMeter", "Shows precisely how accurate you were with each hit.", false, false),  // SHOW_HIT_ERROR_BAR
        B("Always show key overlay", "KeyOverlay", "Show the key overlay when playing instead of only on replays.", false, false),  // ALWAYS_SHOW_KEY_OVERLAY
        B("Load HD images", "LoadHDImages", "Loads HD (@2x) images when available.\nIncreases memory usage and loading times.", true, false),  // LOAD_HD_IMAGES
        Nu("Fixed CS", "FixedCS", "Determines the size of circles and sliders.", 0, 0, 100, false),  // FIXED_CS
        Nu("Fixed HP", "FixedHP", "Determines the rate at which health decreases.", 0, 0, 100, false),  // FIXED_HP
        Nu("Fixed AR", "FixedAR", "Determines how long hit circles stay on the screen.", 0, 0, 100, false),  // FIXED_AR
        Nu("Fixed OD", "FixedOD", "Determines the time window for hit results.", 0, 0, 100, false),  // FIXED_OD
        Nu("Fixed speed", "FixedSpeed", "Determines the speed of the music.", 0, 0, 300, false),  // FIXED_SPEED
        Nu("Track checkpoint", "Checkpoint", "Press Ctrl+L while playing to load a checkpoint, and Ctrl+S to set one.", 0, 0, 1800, false),  // CHECKPOINT
        B("Song list thumbnails", "SongListThumbnails", "Show the beatmap background as a thumbnail on the song list panels. Turn off on low-end devices.", true, false),  // SONG_THUMBNAILS
        B("Parallax", "MenuParallax", "Add a parallax effect based on the current cursor position.", true, false),  // PARALLAX
        B("Theme song", "MenuMusic", "opsu! will play themed music throughout the game, instead of using random beatmaps.", true, false),  // ENABLE_THEME_SONG
        B("Replay seeking", "ReplaySeeking", "Enable a seeking bar on the left side of the screen during replays.", false, false),  // REPLAY_SEEKING
        B("Disable automatic updates", "DisableUpdater", "Disable checking for updates when the game starts.", false, false),  // DISABLE_UPDATER
        B("Watch service", "WatchService", "Watch the beatmap directory for changes. Requires a restart.", false, true),  // ENABLE_WATCH_SERVICE
        B("Enable in-game pause button", "InGamePause", "Displays a pause button during gameplay.", false, false),  // IN_GAME_PAUSE
        B("Beatmap loading screen", "PlayerLoader", "Shows a loading screen with quick settings before a beatmap starts, like osu!lazer.\nLoading pauses while the cursor is over the settings.", true, false),  // PLAYER_LOADER
        Nu("UI Scale", "MobileUIScale", "Scales certain UI elements. Requires a restart.", 0, 5, 25, false),  // MOBILE_UI_SCALING
        Nu("Playfield Scale", "PlayfieldScale", "Scales the playfield. Larger Hitobjects but some may be partially be out of the screen", 0, 10, 20, false),  // PLAYFIELDSCALE
        B("Enable in-game scoreboard", "Scoreboard", "Displays the in-game scoreboard.", false, false),  // SCOREBOARD
        B("Enable in-game average offset display", "AVGOffsetDisp", "Displays an in-game average offset.", false, false),  // AVGOFFSETDISP
        B("Remove some spinner UI elements", "DisableSpinnerUI", "Makes spinners less laggy (maybe).", false, false),  // DISABLE_SPINNER_UI
        B("Disable spinner sounds", "DisableSpinnerSound", "Makes spinners less laggy (maybe).", false, false),  // DISABLE_SPINNER_SOUND
    }};
    return t;
}

const char* const kConstantNames[GameOption::COUNT] = {
    "NULL",
    "BEATMAP_DIRECTORY",
    "IMPORT_DIRECTORY",
    "SCREENSHOT_DIRECTORY",
    "REPLAY_DIRECTORY",
    "SKIN_DIRECTORY",
    "FFMPEG_PATH",
    "THEME_SONG",
    "THEME_SONG_TIMINGPOINT",
    "SCREEN_RESOLUTION",
    "FULLSCREEN",
    "SKIN",
    "TARGET_FPS",
    "SHOW_FPS",
    "SHOW_UNICODE",
    "SCREENSHOT_FORMAT",
    "CURSOR_SIZE",
    "DYNAMIC_BACKGROUND",
    "SHOW_INTRO",
    "LOAD_VERBOSE",
    "MASTER_VOLUME",
    "MUSIC_VOLUME",
    "EFFECT_VOLUME",
    "HITSOUND_VOLUME",
    "MUSIC_OFFSET",
    "RENDER_OFFSET",
    "DISABLE_GAMEPLAY_SOUNDS",
    "DISABLE_SOUNDS",
    "KEY_LEFT",
    "KEY_RIGHT",
    "DISABLE_MOUSE_WHEEL",
    "DISABLE_MOUSE_BUTTONS",
    "MULTITOUCH_GAMEPLAY",
    "TOUCH_MAX_FINGERS",
    "TOUCH_PALM_REJECTION",
    "DISABLE_CURSOR",
    "BACKGROUND_DIM",
    "BACKGROUND_BLUR",
    "FORCE_DEFAULT_PLAYFIELD",
    "ENABLE_STORYBOARD",
    "ENABLE_VIDEOS",
    "IGNORE_BEATMAP_SKINS",
    "FORCE_SKIN_CURSOR",
    "SNAKING_SLIDERS",
    "SLIDER_CAPS",
    "SLIDER_SHRINK",
    "SLIDER_MERGE",
    "SHOW_HIT_LIGHTING",
    "SHOW_HIT_STARS",
    "SHOW_COMBO_BURSTS",
    "SHOW_PERFECT_HIT",
    "SHOW_FOLLOW_POINTS",
    "SHOW_HIT_ERROR_BAR",
    "ALWAYS_SHOW_KEY_OVERLAY",
    "LOAD_HD_IMAGES",
    "FIXED_CS",
    "FIXED_HP",
    "FIXED_AR",
    "FIXED_OD",
    "FIXED_SPEED",
    "CHECKPOINT",
    "SONG_THUMBNAILS",
    "PARALLAX",
    "ENABLE_THEME_SONG",
    "REPLAY_SEEKING",
    "DISABLE_UPDATER",
    "ENABLE_WATCH_SERVICE",
    "IN_GAME_PAUSE",
    "PLAYER_LOADER",
    "MOBILE_UI_SCALING",
    "PLAYFIELDSCALE",
    "SCOREBOARD",
    "AVGOFFSETDISP",
    "DISABLE_SPINNER_UI",
    "DISABLE_SPINNER_SOUND"
};

inline int idx(Id id) { return static_cast<int>(id); }

// Options.Resolution (private enum in the Java): the selectable window sizes, in Java order.
struct Res { int w, h; };
constexpr Res kResolutions[] = {{800, 600},   {1024, 600},  {1024, 768},  {1280, 720},  {1280, 800},  {1280, 960},
                                {1280, 1024}, {1366, 768},  {1440, 900},  {1600, 900},  {1600, 1200}, {1680, 1050},
                                {1920, 1080}, {1920, 1200}, {2560, 1440}, {2560, 1600}, {3840, 2160}};
constexpr int kResolutionCount = static_cast<int>(sizeof(kResolutions) / sizeof(kResolutions[0]));
constexpr int kDefaultResolution = 2;  // RES_1024_768
constexpr int kTargetFPS[] = {60, 120, 240, -1 /* Unlimited */};
constexpr int kTargetFPSCount = 4;
const char* const kScreenshotFormat[] = {"png", "jpg", "bmp"};
constexpr int kScreenshotFormatCount = 3;

std::string resToString(const Res& r) { return std::to_string(r.w) + "x" + std::to_string(r.h); }
// Java hasFullscreenDisplayMode(): the body is commented out in the Java port, so it is always false.
bool hasFullscreenDisplayMode(const Res&) { return false; }

std::string upperAscii(std::string s) {
    for (char& c : s) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}
std::string lowerAscii(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// ---------------------------------------------------------------------------------------------------------------------
// The one state object behind the static facade.
// ---------------------------------------------------------------------------------------------------------------------
struct State {
    std::array<std::atomic<bool>, N> bools;
    std::array<std::atomic<int>, N> vals;
    std::array<std::atomic<int>, N> defaults;   // numeric default (the two screen-size ones are resolved at runtime)
    std::array<std::atomic<bool>, N> visible;
    std::atomic<int> resolutionIndex{kDefaultResolution};
    std::atomic<int> targetFPSIndex{0};
    std::atomic<int> screenshotFormatIndex{0};

    std::mutex keyMu;  // keeps keyLeft != keyRight consistent
    int keyLeft = OptionKeys::KEY_NONE, keyRight = OptionKeys::KEY_NONE;

    std::mutex mu;  // everything below
    std::shared_ptr<const OptionsSeams> seams = std::make_shared<OptionsSeams>();
    std::string skinName = Options::DEFAULT_SKIN_NAME;
    std::string themeString = "theme.mp3,Rainbows,Kevin MacLeod,219350";
    std::string themeTimingPoint = "1120,545.454545454545,4,1,0,100,0,0";
    std::string savedSortOrder;
    bool resolutionListValid = false;
    std::vector<int> resolutionList;  // indices into kResolutions (Java caches this list on first use)

    std::atomic<bool> dirty{false};
    std::atomic<int64_t> dirtySince{0};

    // directories (guarded by dirMu); "" = null in the Java
    std::mutex dirMu;
    bool initialized = false;
    OptionsPaths paths;
    std::string beatmapDir, importDir, screenshotDir, replayDir, skinRootDir, ffmpegPath;
    std::atomic<int> sliderStyleVersion{0};
    std::mutex saveMu;  // Java: saveOptions() is synchronized

    State() { applyDefaults(); }

    std::shared_ptr<const OptionsSeams> getSeams() {
        std::lock_guard<std::mutex> lk(mu);
        return seams;
    }

    void applyDefaults() {
        const auto& m = metas();
        for (int i = 0; i < N; i++) {
            bools[i] = m[i].defBool;
            vals[i] = m[i].defVal;
            defaults[i] = m[i].defVal;
            visible[i] = true;
        }
        resolveScreenDefaults(false);
    }

    // MOBILE_UI_SCALING / PLAYFIELDSCALE default depends on the screen width in inches (<= 6.0 is a phone).
    void resolveScreenDefaults(bool keepCustomized) {
        float inches = 10.0f;
        auto sm = getSeams();
        if (sm->screenWidthInches) inches = sm->screenWidthInches();
        const bool small = inches <= 6.0f;
        setScreenDefault(Id::MOBILE_UI_SCALING, small ? 20 : 10, keepCustomized);
        setScreenDefault(Id::PLAYFIELDSCALE, small ? 16 : 10, keepCustomized);
    }
    void setScreenDefault(Id id, int def, bool keepCustomized) {
        int i = idx(id);
        bool wasDefault = vals[i].load() == defaults[i].load();
        defaults[i] = def;
        if (!keepCustomized || wasDefault) vals[i] = def;
    }

    void clearAll() {
        applyDefaults();
        resolutionIndex = kDefaultResolution;
        targetFPSIndex = 0;
        screenshotFormatIndex = 0;
        { std::lock_guard<std::mutex> lk(keyMu); keyLeft = keyRight = OptionKeys::KEY_NONE; }
        std::lock_guard<std::mutex> lk(mu);
        seams = std::make_shared<OptionsSeams>();
        skinName = Options::DEFAULT_SKIN_NAME;
        themeString = "theme.mp3,Rainbows,Kevin MacLeod,219350";
        themeTimingPoint = "1120,545.454545454545,4,1,0,100,0,0";
        savedSortOrder.clear();
        resolutionListValid = false;
        resolutionList.clear();
        dirty = false;
        dirtySince = 0;
        sliderStyleVersion = 0;
        std::lock_guard<std::mutex> dl(dirMu);
        initialized = false;
        paths = OptionsPaths();
        beatmapDir.clear(); importDir.clear(); screenshotDir.clear(); replayDir.clear(); skinRootDir.clear(); ffmpegPath.clear();
    }
};

State& st() {
    static State s;  // lazily constructed on first use; no I/O
    return s;
}

int64_t nowMs() {
    auto sm = st().getSeams();
    if (sm->nowMs) return sm->nowMs();
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void notify(const std::string& msg) {
    auto sm = st().getSeams();
    if (sm->sendBarNotification) sm->sendBarNotification(msg);
}

bool atomicToggle(std::atomic<bool>& b) {
    bool e = b.load();
    while (!b.compare_exchange_weak(e, !e)) {}
    return !e;
}

std::string fmtD(int v) { return std::to_string(v); }
// String.format("%.Nf", val / D) with D a float literal: the int is converted to float, divided as float, then widened.
std::string fmtF(int val, float div, int prec) {
    float f = static_cast<float>(val) / div;
    return java::formatFixed(static_cast<double>(f), prec);
}

// Resolution list shown in the settings (Java caches it in the enum constant).
const std::vector<int>& resolutionListLocked(State& s, std::unique_lock<std::mutex>& lk) {
    (void)lk;
    if (!s.resolutionListValid) {
        int width = 0, height = 0;
        if (s.seams->desktopDisplaySize) {
            auto p = s.seams->desktopDisplaySize();
            width = p.first; height = p.second;
        }
        s.resolutionList.clear();
        for (int i = 0; i < kResolutionCount; i++) {
            // only show resolutions that fit on the screen
            if (i == 0 || (width >= kResolutions[i].w && height >= kResolutions[i].h)) s.resolutionList.push_back(i);
        }
        s.resolutionListValid = true;
    }
    return s.resolutionList;
}

std::vector<std::string> resolutionItemNames() {
    State& s = st();
    std::unique_lock<std::mutex> lk(s.mu);
    // call the seam without holding our lock would be nicer, but the list is built once; the seam must not call back.
    const auto& l = resolutionListLocked(s, lk);
    std::vector<std::string> out;
    for (int i : l) out.push_back(resToString(kResolutions[i]));
    return out;
}

// Theme song string check (Java getThemeBeatmap() != null): 4 comma separated tokens, the 4th an int, and the timing
// point string must parse. audioName receives tokens[0].
bool themeStringValid(const std::string& themeString, const std::string& timingPoint, std::string* audioName) {
    std::vector<std::string> tokens = java::split(themeString, ',');
    if (tokens.size() != 4) return false;
    try {
        (void)java::parseInt(tokens[3]);
    } catch (const java::NumberFormatException&) {
        return false;
    }
    try {
        TimingPoint tp(timingPoint);
        (void)tp;
    } catch (const std::exception&) {
        return false;
    }
    if (audioName) *audioName = tokens[0];
    return true;
}

bool isValidGameKey(int key) {
    using namespace OptionKeys;
    return key > 0 && key != ANDROID_BACK && key != MOUSE_BACK_BUTTON && key != KEY_ESCAPE && key != KEY_SPACE &&
           key != KEY_UP && key != KEY_DOWN && key != KEY_F7 && key != KEY_F10 && key != KEY_F12;
}

int parseFixed(const std::string& s, float mult) {
    float f = java::parseFloat(s);
    return java::toInt(static_cast<double>(f * mult));
}

}  // namespace

// =====================================================================================================================
// GameOption
// =====================================================================================================================

namespace {
template <size_t... I>
std::array<GameOption, GameOption::COUNT> makeValues(std::index_sequence<I...>) {
    return {{GameOption(static_cast<GameOption::Id>(I))...}};
}
}  // namespace

const std::array<GameOption, GameOption::COUNT>& GameOption::values() {
    static const std::array<GameOption, COUNT> v = makeValues(std::make_index_sequence<COUNT>());
    return v;
}

const char* GameOption::constantName() const { return kConstantNames[ordinal()]; }

bool GameOption::fromConstantName(const std::string& constantName, GameOption& out) {
    for (int i = 0; i < COUNT; i++) {
        if (constantName == kConstantNames[i]) {
            out = GameOption(static_cast<Id>(i));
            return true;
        }
    }
    return false;
}

const std::string& GameOption::getName() const { return metas()[ordinal()].name; }
const std::string& GameOption::getDisplayName() const { return metas()[ordinal()].key; }
const std::string& GameOption::getDescription() const { return metas()[ordinal()].desc; }
GameOption::OptionType GameOption::getType() const { return metas()[ordinal()].type; }
bool GameOption::isRestartRequired() const { return metas()[ordinal()].restart; }

bool GameOption::getBooleanValue() const { return st().bools[ordinal()].load(); }
int GameOption::getIntegerValue() const { return st().vals[ordinal()].load(); }
int GameOption::getDefaultIntegerValue() const { return st().defaults[ordinal()].load(); }
bool GameOption::isCustomized() const {
    return getType() == OptionType::NUMERIC && getIntegerValue() != getDefaultIntegerValue();
}
void GameOption::resetToDefault() const {
    if (getType() == OptionType::NUMERIC) setValue(getDefaultIntegerValue());
}
int GameOption::getMinValue() const { return metas()[ordinal()].min; }
int GameOption::getMaxValue() const { return metas()[ordinal()].max; }

std::vector<std::string> GameOption::getItemList() const {
    switch (id_) {
        case Id::SCREEN_RESOLUTION:
            return resolutionItemNames();
        case Id::SKIN: {
            // Java re-reads the directory every time so freshly imported skins show up.
            std::string root = Options::getSkinRootDir();
            std::vector<std::string> out{Options::DEFAULT_SKIN_NAME};
            auto sm = st().getSeams();
            if (sm->listSkinDirectories)
                for (auto& n : sm->listSkinDirectories(root)) out.push_back(n);
            return out;
        }
        case Id::TARGET_FPS: {
            std::vector<std::string> out;
            for (int fps : kTargetFPS)
                out.push_back(fps == -1 ? "Unlimited" : (fps == 60 ? fmtD(fps) + "fps (vsync)" : fmtD(fps) + "fps"));
            return out;
        }
        case Id::SCREENSHOT_FORMAT: {
            std::vector<std::string> out;
            for (const char* f : kScreenshotFormat) out.push_back(upperAscii(f));
            return out;
        }
        default:
            return {};
    }
}

void GameOption::setValue(bool value) const {
    st().bools[ordinal()] = value;
    Options::markDirty();
}

void GameOption::setValue(int value) const {
    const Meta& m = metas()[ordinal()];
    st().vals[ordinal()] = Utils::clamp(value, m.min, m.max);
    Options::markDirty();
    if (id_ == Id::MASTER_VOLUME || id_ == Id::MUSIC_VOLUME) {
        auto sm = st().getSeams();
        if (sm->onVolumeAdjusted) sm->onVolumeAdjusted(Options::getMasterVolume() * Options::getMusicVolume());
    }
}

void GameOption::toggle(IGameContainer*) const {
    switch (id_) {
        case Id::FULLSCREEN: {
            const Res& r = kResolutions[st().resolutionIndex.load()];
            // check if fullscreen mode is possible with this resolution
            if (!getBooleanValue() && !hasFullscreenDisplayMode(r)) {
                notify("Fullscreen mode is not available at resolution " + resToString(r));
                return;
            }
            atomicToggle(st().bools[ordinal()]);
            return;
        }
        case Id::SHOW_FPS: {
            atomicToggle(st().bools[ordinal()]);
            auto sm = st().getSeams();
            if (sm->resetFPSDisplay) sm->resetFPSDisplay();
            return;
        }
        case Id::SHOW_UNICODE: {
            bool now = atomicToggle(st().bools[ordinal()]);
            if (now) {
                auto sm = st().getSeams();
                if (sm->loadUnicodeGlyphs) sm->loadUnicodeGlyphs();
            }
            return;
        }
        default:
            atomicToggle(st().bools[ordinal()]);
            return;
    }
}

void GameOption::selectItem(int index, IGameContainer* container) const {
    State& s = st();
    switch (id_) {
        case Id::SCREEN_RESOLUTION: {
            int chosen;
            {
                std::unique_lock<std::mutex> lk(s.mu);
                const auto& l = resolutionListLocked(s, lk);
                if (index < 0 || index >= static_cast<int>(l.size())) return;  // Java: AIOOBE
                chosen = l[static_cast<size_t>(index)];
            }
            s.resolutionIndex = chosen;
            // check if fullscreen mode is possible with this resolution
            if (FULLSCREEN.getBooleanValue() && !hasFullscreenDisplayMode(kResolutions[chosen])) FULLSCREEN.toggle(container);
            return;
        }
        case Id::SKIN: {
            auto items = getItemList();
            if (index < 0 || index >= static_cast<int>(items.size())) return;
            auto sm = s.getSeams();
            if (sm->requestSkin) sm->requestSkin(items[static_cast<size_t>(index)]);  // fades out, swaps, fades back in
            return;
        }
        case Id::TARGET_FPS: {
            if (index < 0 || index >= kTargetFPSCount) return;  // Java: AIOOBE on the next getTargetFPS()
            s.targetFPSIndex = index;
            int fps = kTargetFPS[index];
            bool vsync = (fps == 60);
            if (container) {
                container->setTargetFrameRate(fps);
                container->setVSync(vsync);
            }
            return;
        }
        case Id::SCREENSHOT_FORMAT:
            if (index >= 0 && index < kScreenshotFormatCount) s.screenshotFormatIndex = index;
            return;
        default:
            return;
    }
}

std::string GameOption::getValueString() const {
    State& s = st();
    const int v = getIntegerValue();
    switch (id_) {
        case Id::SCREEN_RESOLUTION:
            return resToString(kResolutions[s.resolutionIndex.load()]);
        case Id::SKIN: {
            std::lock_guard<std::mutex> lk(s.mu);
            return s.skinName;
        }
        case Id::TARGET_FPS: {
            int fps = Options::getTargetFPS();
            return fps == -1 ? "Unlimited" : (fps == 60 ? fmtD(fps) + "fps (vsync)" : fmtD(fps) + "fps");
        }
        case Id::SCREENSHOT_FORMAT:
            return upperAscii(kScreenshotFormat[s.screenshotFormatIndex.load()]);
        case Id::CURSOR_SIZE:
            return fmtF(v, 100.0f, 2) + "x";
        case Id::MUSIC_OFFSET:
            return fmtD(v) + " ms";
        case Id::RENDER_OFFSET:
            return fmtD(v) + "ms";
        case Id::KEY_LEFT:
            return Options::keyName(Options::getGameKeyLeft());
        case Id::KEY_RIGHT:
            return Options::keyName(Options::getGameKeyRight());
        case Id::TOUCH_MAX_FINGERS:
            return fmtD(v);
        case Id::FIXED_CS:
        case Id::FIXED_HP:
        case Id::FIXED_AR:
        case Id::FIXED_OD:
        case Id::MOBILE_UI_SCALING:
        case Id::PLAYFIELDSCALE:
            // MOBILE_UI_SCALING / PLAYFIELDSCALE print "Disabled" for 0 too (unreachable: their min is 5 / 10)
            return v == 0 ? "Disabled" : fmtF(v, 10.0f, 1);
        case Id::FIXED_SPEED:
            return v == 0 ? "Disabled" : fmtF(v, 100.0f, 2) + "x";
        case Id::CHECKPOINT: {
            if (v == 0) return "Disabled";
            int minutes = v / 60;  // TimeUnit.SECONDS.toMinutes (val is never negative here)
            int seconds = v - minutes * 60;
            char buf[32];
            std::snprintf(buf, sizeof buf, "%02d:%02d", minutes, seconds);
            return buf;
        }
        default:
            break;
    }
    switch (getType()) {
        case OptionType::NUMERIC:
            return fmtD(v) + "%";
        case OptionType::BOOLEAN:
            return getBooleanValue() ? "Yes" : "No";
        default:
            return "";
    }
}

std::string GameOption::write() const {
    State& s = st();
    const int v = getIntegerValue();
    switch (id_) {
        case Id::BEATMAP_DIRECTORY: return Options::getBeatmapDir();
        case Id::IMPORT_DIRECTORY: return Options::getImportDir();
        case Id::SCREENSHOT_DIRECTORY: return Options::getScreenshotDir();
        case Id::REPLAY_DIRECTORY: return Options::getReplayDir();
        case Id::SKIN_DIRECTORY: return Options::getSkinRootDir();
        case Id::FFMPEG_PATH: return Options::getFFmpegLocation();
        case Id::THEME_SONG: {
            std::lock_guard<std::mutex> lk(s.mu);
            return s.themeString;
        }
        case Id::THEME_SONG_TIMINGPOINT: {
            std::lock_guard<std::mutex> lk(s.mu);
            return s.themeTimingPoint;
        }
        case Id::TARGET_FPS:
            return fmtD(Options::getTargetFPS());
        case Id::SCREENSHOT_FORMAT:
            return fmtD(s.screenshotFormatIndex.load());
        // Locale.US formatting (same digits as getValueString for these)
        case Id::CURSOR_SIZE:
            return fmtF(v, 100.0f, 2);
        case Id::FIXED_CS:
        case Id::FIXED_HP:
        case Id::FIXED_AR:
        case Id::FIXED_OD:
        case Id::MOBILE_UI_SCALING:
        case Id::PLAYFIELDSCALE:
            return fmtF(v, 10.0f, 1);
        case Id::FIXED_SPEED:
            return fmtF(v, 100.0f, 2);
        case Id::KEY_LEFT:
            return Options::keyName(Options::getGameKeyLeft());
        case Id::KEY_RIGHT:
            return Options::keyName(Options::getGameKeyRight());
        default:
            break;
    }
    switch (getType()) {
        case OptionType::NUMERIC:
            return fmtD(v);
        case OptionType::BOOLEAN:
            return getBooleanValue() ? "true" : "false";
        default:
            return getValueString();  // SELECT: RESOLUTION ("1024x768"), SKIN (name)
    }
}

void GameOption::read(const std::string& str) const {
    State& s = st();
    const Meta& m = metas()[ordinal()];
    switch (id_) {
        case Id::BEATMAP_DIRECTORY: { std::lock_guard<std::mutex> lk(s.dirMu); s.beatmapDir = str; return; }
        case Id::IMPORT_DIRECTORY: { std::lock_guard<std::mutex> lk(s.dirMu); s.importDir = str; return; }
        case Id::SCREENSHOT_DIRECTORY: { std::lock_guard<std::mutex> lk(s.dirMu); s.screenshotDir = str; return; }
        case Id::REPLAY_DIRECTORY: { std::lock_guard<std::mutex> lk(s.dirMu); s.replayDir = str; return; }
        case Id::SKIN_DIRECTORY: { std::lock_guard<std::mutex> lk(s.dirMu); s.skinRootDir = str; return; }
        case Id::FFMPEG_PATH: {
            if (!str.empty()) { std::lock_guard<std::mutex> lk(s.dirMu); s.ffmpegPath = str; }
            return;
        }
        case Id::THEME_SONG: {
            std::string oldTheme, timing, audioName;
            {
                std::lock_guard<std::mutex> lk(s.mu);
                oldTheme = s.themeString;
                timing = s.themeTimingPoint;
                s.themeString = str;
            }
            if (!themeStringValid(str, timing, &audioName)) {
                std::lock_guard<std::mutex> lk(s.mu);
                s.themeString = oldTheme;
                Log::warn("The theme song string [" + str + "] is malformed.");
            } else {
                // !beatmap.audioFilename.isFile() && !ResourceLoader.resourceExists(audioFilename.getName())
                std::error_code ec;
                bool isFile = std::filesystem::is_regular_file(std::filesystem::path(audioName), ec);
                std::string baseName = std::filesystem::path(audioName).filename().string();
                auto sm = s.getSeams();
                bool bundled = sm->bundledResourceExists && sm->bundledResourceExists(baseName);
                if (!isFile && !bundled) {
                    std::lock_guard<std::mutex> lk(s.mu);
                    s.themeString = oldTheme;
                    Log::warn("Cannot find theme song [" + audioName + "].");
                }
            }
            return;
        }
        case Id::THEME_SONG_TIMINGPOINT: {
            try {
                TimingPoint tp(str);
                (void)tp;
                std::lock_guard<std::mutex> lk(s.mu);
                s.themeTimingPoint = str;
            } catch (const std::exception&) {
                Log::warn("The theme song timing point [" + str + "] is malformed.");
            }
            return;
        }
        case Id::SCREEN_RESOLUTION: {
            // Resolution.valueOf("RES_" + s.replace('x', '_')): unknown names are ignored
            std::string name = str;
            std::replace(name.begin(), name.end(), 'x', '_');
            name = "RES_" + name;
            for (int i = 0; i < kResolutionCount; i++) {
                if (name == "RES_" + std::to_string(kResolutions[i].w) + "_" + std::to_string(kResolutions[i].h)) {
                    s.resolutionIndex = i;
                    break;
                }
            }
            return;
        }
        case Id::SKIN: {
            std::lock_guard<std::mutex> lk(s.mu);
            s.skinName = str;
            return;
        }
        case Id::TARGET_FPS: {
            int i = java::parseInt(str);
            for (int j = 0; j < kTargetFPSCount; j++) {
                if (i == kTargetFPS[j]) {
                    s.targetFPSIndex = j;
                    break;
                }
            }
            return;
        }
        case Id::SCREENSHOT_FORMAT: {
            int i = java::parseInt(str);
            if (i >= 0 && i < kScreenshotFormatCount) s.screenshotFormatIndex = i;
            return;
        }
        case Id::CURSOR_SIZE:
        case Id::FIXED_SPEED: {
            int i = parseFixed(str, 100.0f);
            if (i >= m.min && i <= m.max) s.vals[ordinal()] = i;
            return;
        }
        case Id::FIXED_CS:
        case Id::FIXED_HP:
        case Id::FIXED_AR:
        case Id::FIXED_OD:
        case Id::MOBILE_UI_SCALING:
        case Id::PLAYFIELDSCALE: {
            int i = parseFixed(str, 10.0f);
            if (i >= m.min && i <= m.max) s.vals[ordinal()] = i;  // Java hard-codes 5..25 / 10..20 = min..max
            return;
        }
        case Id::KEY_LEFT:
            Options::setGameKeyLeft(Options::keyIndex(str));
            return;
        case Id::KEY_RIGHT:
            Options::setGameKeyRight(Options::keyIndex(str));
            return;
        default:
            break;
    }
    if (m.type == OptionType::NUMERIC) {
        int i = java::parseInt(str);
        if (i >= m.min && i <= m.max) s.vals[ordinal()] = i;
    } else if (m.type == OptionType::BOOLEAN) {
        s.bools[ordinal()] = (lowerAscii(str) == "true");  // Boolean.parseBoolean
    }
}

bool GameOption::matches(const std::string& query) const {
    return !query.empty() && (lowerAscii(getName()).find(query) != std::string::npos ||
                              lowerAscii(getDescription()).find(query) != std::string::npos);
}

void GameOption::setVisible(bool visible) const { st().visible[ordinal()] = visible; }
bool GameOption::isVisible() const { return st().visible[ordinal()].load(); }

// =====================================================================================================================
// Options
// =====================================================================================================================

void Options::setSeams(OptionsSeams seams) {
    State& s = st();
    {
        std::lock_guard<std::mutex> lk(s.mu);
        s.seams = std::make_shared<const OptionsSeams>(std::move(seams));
        s.resolutionListValid = false;
    }
    s.resolveScreenDefaults(true);
}

void Options::resetForTests() { st().clearAll(); }

int Options::getTargetFPS() { return kTargetFPS[st().targetFPSIndex.load()]; }

void Options::setNextFPS(IGameContainer* container) {
    int index = (st().targetFPSIndex.load() + 1) % kTargetFPSCount;
    if (index == kTargetFPSCount - 1) index = 0;  // Skip "Unlimited" option
    GameOption::TARGET_FPS.selectItem(index, container);
    notify("Frame limiter: " + GameOption::TARGET_FPS.getValueString());
}

float Options::getMasterVolume() { return GameOption::MASTER_VOLUME.getIntegerValue() / 100.0f; }

void Options::setMasterVolume(IGameContainer*, float volume) {
    if (volume >= 0.0f && volume <= 1.0f) {
        GameOption::MASTER_VOLUME.setValue(java::toInt(static_cast<double>(volume * 100.0f)));
        auto sm = st().getSeams();
        if (sm->setMusicControllerVolume) sm->setMusicControllerVolume(getMasterVolume() * getMusicVolume());
    }
}

float Options::getMusicVolume() { return GameOption::MUSIC_VOLUME.getIntegerValue() / 100.0f; }
float Options::getEffectVolume() { return GameOption::EFFECT_VOLUME.getIntegerValue() / 100.0f; }
float Options::getHitSoundVolume() { return GameOption::HITSOUND_VOLUME.getIntegerValue() / 100.0f; }
int Options::getMusicOffset() { return GameOption::MUSIC_OFFSET.getIntegerValue(); }
int Options::getRenderOffset() { return GameOption::RENDER_OFFSET.getIntegerValue(); }
std::string Options::getScreenshotFormat() { return kScreenshotFormat[st().screenshotFormatIndex.load()]; }

Resolution Options::getResolution() {
    const Res& r = kResolutions[st().resolutionIndex.load()];
    return Resolution(r.w, r.h);
}

void Options::setDisplayMode(IDisplayHost& app) {
    State& s = st();
    int screenWidth = app.getScreenWidth();
    int screenHeight = app.getScreenHeight();
    bool fullscreen = isFullscreen();
    int ri = s.resolutionIndex.load();

    // check for larger-than-screen dimensions
    if (screenWidth < kResolutions[ri].w || screenHeight < kResolutions[ri].h) {
        ri = 0;  // RES_800_600
        s.resolutionIndex = ri;
    }
    // check if fullscreen mode is possible with this resolution
    if (fullscreen && !hasFullscreenDisplayMode(kResolutions[ri])) fullscreen = false;

    try {
        app.setDisplayMode(kResolutions[ri].w, kResolutions[ri].h, fullscreen);
    } catch (const std::exception& e) {
        Log::error(std::string("Failed to set display mode: ") + e.what());  // Java: ErrorHandler.error(..., true)
    }
    // (Java then sets the LWJGL system property org.lwjgl.opengl.Window.undecorated: desktop LWJGL only, dropped.)
}

bool Options::isFullscreen() { return GameOption::FULLSCREEN.getBooleanValue(); }
bool Options::isFPSCounterEnabled() { return GameOption::SHOW_FPS.getBooleanValue(); }
void Options::toggleFPSCounter() { GameOption::SHOW_FPS.toggle(nullptr); }
bool Options::isHitLightingEnabled() { return GameOption::SHOW_HIT_LIGHTING.getBooleanValue(); }
bool Options::isHitStarsEnabled() { return GameOption::SHOW_HIT_STARS.getBooleanValue(); }
bool Options::isComboBurstEnabled() { return GameOption::SHOW_COMBO_BURSTS.getBooleanValue(); }
float Options::getCursorScale() { return GameOption::CURSOR_SIZE.getIntegerValue() / 100.0f; }
bool Options::isDynamicBackgroundEnabled() { return GameOption::DYNAMIC_BACKGROUND.getBooleanValue(); }
bool Options::isPerfectHitBurstEnabled() { return GameOption::SHOW_PERFECT_HIT.getBooleanValue(); }
bool Options::isFollowPointEnabled() { return GameOption::SHOW_FOLLOW_POINTS.getBooleanValue(); }
float Options::getBackgroundDim() { return (100 - GameOption::BACKGROUND_DIM.getIntegerValue()) / 100.0f; }
int Options::getBackgroundBlur() { return GameOption::BACKGROUND_BLUR.getIntegerValue(); }
bool Options::isDefaultPlayfieldForced() { return GameOption::FORCE_DEFAULT_PLAYFIELD.getBooleanValue(); }
bool Options::isStoryboardEnabled() { return GameOption::ENABLE_STORYBOARD.getBooleanValue(); }
bool Options::isBeatmapVideoEnabled() { return GameOption::ENABLE_VIDEOS.getBooleanValue(); }
bool Options::isBeatmapSkinIgnored() { return GameOption::IGNORE_BEATMAP_SKINS.getBooleanValue(); }
bool Options::isSkinCursorForced() { return GameOption::FORCE_SKIN_CURSOR.getBooleanValue(); }
bool Options::isSliderSnaking() { return GameOption::SNAKING_SLIDERS.getBooleanValue(); }
bool Options::isSliderCapsDrawn() { return GameOption::SLIDER_CAPS.getBooleanValue(); }
bool Options::isSliderShrinking() { return GameOption::SLIDER_SHRINK.getBooleanValue(); }
bool Options::isSliderMerging() { return GameOption::SLIDER_MERGE.getBooleanValue(); }
float Options::getFixedCS() { return GameOption::FIXED_CS.getIntegerValue() / 10.0f; }
float Options::getFixedHP() { return GameOption::FIXED_HP.getIntegerValue() / 10.0f; }
float Options::getFixedAR() { return GameOption::FIXED_AR.getIntegerValue() / 10.0f; }
float Options::getFixedOD() { return GameOption::FIXED_OD.getIntegerValue() / 10.0f; }
float Options::getFixedSpeed() { return GameOption::FIXED_SPEED.getIntegerValue() / 100.0f; }
bool Options::isLoadVerbose() { return GameOption::LOAD_VERBOSE.getBooleanValue(); }
bool Options::isIntroEnabled() { return GameOption::SHOW_INTRO.getBooleanValue(); }
int Options::getCheckpoint() { return GameOption::CHECKPOINT.getIntegerValue() * 1000; }
bool Options::isGameplaySoundDisabled() { return GameOption::DISABLE_GAMEPLAY_SOUNDS.getBooleanValue(); }
bool Options::isSoundDisabled() { return GameOption::DISABLE_SOUNDS.getBooleanValue(); }
bool Options::useUnicodeMetadata() { return GameOption::SHOW_UNICODE.getBooleanValue(); }
bool Options::isSongThumbnailsEnabled() { return GameOption::SONG_THUMBNAILS.getBooleanValue(); }
bool Options::isParallaxEnabled() { return GameOption::PARALLAX.getBooleanValue(); }
bool Options::isThemeSongEnabled() { return GameOption::ENABLE_THEME_SONG.getBooleanValue(); }
bool Options::isReplaySeekingEnabled() { return GameOption::REPLAY_SEEKING.getBooleanValue(); }
bool Options::isUpdaterDisabled() { return GameOption::DISABLE_UPDATER.getBooleanValue(); }
bool Options::isWatchServiceEnabled() { return GameOption::ENABLE_WATCH_SERVICE.getBooleanValue(); }

bool Options::setCheckpoint(int time) {
    if (time >= 0 && time < 3600) {
        GameOption::CHECKPOINT.setValue(time);  // clamped to the option's max (1800) like the Java
        return true;
    }
    return false;
}

bool Options::isHitErrorBarEnabled() { return GameOption::SHOW_HIT_ERROR_BAR.getBooleanValue(); }
bool Options::alwaysShowKeyOverlay() { return GameOption::ALWAYS_SHOW_KEY_OVERLAY.getBooleanValue(); }
bool Options::loadHDImages() { return GameOption::LOAD_HD_IMAGES.getBooleanValue(); }
bool Options::isMouseWheelDisabled() { return GameOption::DISABLE_MOUSE_WHEEL.getBooleanValue(); }
bool Options::isMouseDisabled() { return GameOption::DISABLE_MOUSE_BUTTONS.getBooleanValue(); }

bool Options::isMultitouchGameplay() {
    if (!GameOption::MULTITOUCH_GAMEPLAY.getBooleanValue()) return false;
    auto sm = st().getSeams();
    return sm->isTouchPlatform && sm->isTouchPlatform();
}

bool Options::isDesktopPlatform() {
    auto sm = st().getSeams();
    return sm->isDesktop && sm->isDesktop();
}

bool Options::isTouchPalmRejection() { return GameOption::TOUCH_PALM_REJECTION.getBooleanValue(); }
int Options::getMaxTouchFingers() { return GameOption::TOUCH_MAX_FINGERS.getIntegerValue(); }

void Options::toggleMouseDisabled() {
    GameOption::DISABLE_MOUSE_BUTTONS.toggle(nullptr);
    notify(GameOption::DISABLE_MOUSE_BUTTONS.getBooleanValue() ? "Mouse buttons are disabled." : "Mouse buttons are enabled.");
}

bool Options::isCursorDisabled() { return GameOption::DISABLE_CURSOR.getBooleanValue(); }

int Options::getGameKeyLeft() {
    {
        std::lock_guard<std::mutex> lk(st().keyMu);
        if (st().keyLeft != OptionKeys::KEY_NONE) return st().keyLeft;
    }
    setGameKeyLeft(OptionKeys::KEY_Z);
    std::lock_guard<std::mutex> lk(st().keyMu);
    return st().keyLeft;
}

int Options::getGameKeyRight() {
    {
        std::lock_guard<std::mutex> lk(st().keyMu);
        if (st().keyRight != OptionKeys::KEY_NONE) return st().keyRight;
    }
    setGameKeyRight(OptionKeys::KEY_X);
    std::lock_guard<std::mutex> lk(st().keyMu);
    return st().keyRight;
}

bool Options::setGameKeyLeft(int key) {
    std::lock_guard<std::mutex> lk(st().keyMu);
    if ((key == st().keyRight && key != OptionKeys::KEY_NONE) || !isValidGameKey(key)) return false;
    st().keyLeft = key;
    return true;
}

bool Options::setGameKeyRight(int key) {
    std::lock_guard<std::mutex> lk(st().keyMu);
    if ((key == st().keyLeft && key != OptionKeys::KEY_NONE) || !isValidGameKey(key)) return false;
    st().keyRight = key;
    return true;
}

float Options::getMobileUIScale() { return GameOption::MOBILE_UI_SCALING.getIntegerValue() / 10.0f; }
float Options::getPlayfieldScale() { return GameOption::PLAYFIELDSCALE.getIntegerValue() / 10.0f; }
float Options::getMobileUIScale(float scale) { return 1 + ((getMobileUIScale() - 1) * scale); }
float Options::getMobileUIScaleHigh() { return getMobileUIScale() > 1 ? getMobileUIScale() : getMobileUIScale(0.5f); }
bool Options::isInGamePauseEnabled() { return GameOption::IN_GAME_PAUSE.getBooleanValue(); }
bool Options::isPlayerLoaderEnabled() { return GameOption::PLAYER_LOADER.getBooleanValue(); }

std::string Options::getSkinName() {
    std::lock_guard<std::mutex> lk(st().mu);
    return st().skinName;
}
void Options::setSkinName(const std::string& name) {
    std::lock_guard<std::mutex> lk(st().mu);
    st().skinName = name;
}

std::string Options::getSavedSortOrder() {
    std::lock_guard<std::mutex> lk(st().mu);
    return st().savedSortOrder;
}
void Options::setSavedSortOrder(const std::string& name) {
    {
        std::lock_guard<std::mutex> lk(st().mu);
        if (name.empty() || name == st().savedSortOrder) return;  // Java: name == null || equals
        st().savedSortOrder = name;
    }
    markDirty();
}

void Options::markDirty() {
    st().dirtySince = nowMs();
    st().dirty = true;
}
bool Options::isDirty() { return st().dirty.load(); }
int64_t Options::getDirtySinceMs() { return st().dirtySince.load(); }

std::string Options::keyName(int keycode) {
    if (keycode <= 0) return "Null";
    auto sm = st().getSeams();
    return sm->keyName ? sm->keyName(keycode) : defaultKeyName(keycode);
}
int Options::keyIndex(const std::string& name) {
    auto sm = st().getSeams();
    return sm->keyIndex ? sm->keyIndex(name) : defaultKeyIndex(name);
}

// libGDX numbering: A..Z = 29..54, NUM_0..NUM_9 = 7..16, F1..F12 = 131..142. Names as libGDX Keys.toString gives them
// for these keys ("A", "0", "F1"). Everything else needs the keyName seam (see notes).
std::string Options::defaultKeyName(int keycode) {
    if (keycode >= 29 && keycode <= 54) return std::string(1, static_cast<char>('A' + (keycode - 29)));
    if (keycode >= 7 && keycode <= 16) return std::string(1, static_cast<char>('0' + (keycode - 7)));
    if (keycode >= 131 && keycode <= 142) return "F" + std::to_string(keycode - 130);
    return "Unknown";
}
int Options::defaultKeyIndex(const std::string& name) {
    for (int k = 7; k <= 16; k++) if (defaultKeyName(k) == name) return k;
    for (int k = 29; k <= 54; k++) if (defaultKeyName(k) == name) return k;
    for (int k = 131; k <= 142; k++) if (defaultKeyName(k) == name) return k;
    return -1;
}


// =====================================================================================================================
// Directories and files
// =====================================================================================================================

namespace {

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

// Java File.getAbsolutePath() for the paths we handle: absolute, '/'-separated, no trailing slash, no "..".
std::string absPath(const std::string& p) {
    if (p.empty()) return "";
    std::error_code ec;
    fs::path a = fs::absolute(fs::path(p), ec);
    if (ec) a = fs::path(p);
    std::string r = a.lexically_normal().generic_string();
    while (r.size() > 1 && r.back() == '/') r.pop_back();
    return r;
}

bool isDir(const std::string& p) {
    std::error_code ec;
    return !p.empty() && fs::is_directory(fs::path(p), ec);
}

// Java: if (!dir.isDirectory() && !dir.mkdir()) ErrorHandler.error(...). create_directories also makes missing parents.
void ensureDir(const std::string& p, const char* what) {
    if (isDir(p)) return;
    std::error_code ec;
    fs::create_directories(fs::path(p), ec);
    if (!isDir(p)) Log::error(std::string("Failed to create ") + what + " directory at '" + absPath(p) + "'.");
}

// Snapshot of the base folders ("" members before init()).
OptionsPaths pathsSnapshot(bool* initialized = nullptr) {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.dirMu);
    if (initialized) *initialized = s.initialized;
    return s.paths;
}

}  // namespace

void Options::init(const OptionsPaths& paths) {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.dirMu);
    s.paths = paths;
    s.initialized = true;
    s.beatmapDir.clear(); s.importDir.clear(); s.screenshotDir.clear(); s.replayDir.clear(); s.skinRootDir.clear();
    s.ffmpegPath.clear();
}

bool Options::isInitialized() {
    bool init = false;
    pathsSnapshot(&init);
    return init;
}

#define OPSU_PATH_GETTER(NAME, BASE, SUB)                       \
    std::string Options::NAME() {                               \
        bool init = false;                                      \
        OptionsPaths p = pathsSnapshot(&init);                  \
        return init ? absPath(joinPath(p.BASE, SUB)) : "";      \
    }
std::string Options::getConfigDir() { bool i = false; OptionsPaths p = pathsSnapshot(&i); return i ? absPath(p.configDir) : ""; }
std::string Options::getDataDir() { bool i = false; OptionsPaths p = pathsSnapshot(&i); return i ? absPath(p.dataDir) : ""; }
std::string Options::getCacheDir() { bool i = false; OptionsPaths p = pathsSnapshot(&i); return i ? absPath(p.cacheDir) : ""; }
OPSU_PATH_GETTER(getLogFile, configDir, "opsu.log")
OPSU_PATH_GETTER(getOptionsFile, configDir, "opsu.cfg")
OPSU_PATH_GETTER(getOldOptionsFile, configDir, "../.opsu.cfg")
OPSU_PATH_GETTER(getBeatmapDB, dataDir, "opsu.db")
OPSU_PATH_GETTER(getOldBeatmapDB, dataDir, ".opsu.db")
OPSU_PATH_GETTER(getLocalBeatmapDB, localDir, "opsu.db")
OPSU_PATH_GETTER(getScoreDB, dataDir, "opsu_scores.db")
OPSU_PATH_GETTER(getOldScoreDB, dataDir, ".opsu_scores.db")
OPSU_PATH_GETTER(getLocalScoreDB, localDir, "opsu_scores.db")
OPSU_PATH_GETTER(getNativeDir, cacheDir, "Natives")
OPSU_PATH_GETTER(getTempDir, cacheDir, "Temp")
#undef OPSU_PATH_GETTER

std::string Options::getBeatmapDir() {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.dirMu);
    if (!s.initialized) return "";
    if (!s.beatmapDir.empty() && isDir(s.beatmapDir)) return absPath(s.beatmapDir);
    // (Java would try the osu! installation directory here; getOsuInstallationDirectory() always returns null in this port.)
    s.beatmapDir = absPath(joinPath(s.paths.dataDir, "Songs"));
    ensureDir(s.beatmapDir, "beatmap");
    return s.beatmapDir;
}

std::string Options::getImportDir() {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.dirMu);
    if (!s.initialized) return "";
    if (!s.importDir.empty() && isDir(s.importDir)) return absPath(s.importDir);
    s.importDir = absPath(joinPath(s.paths.dataDir, "Import"));
    ensureDir(s.importDir, "import");
    return s.importDir;
}

std::string Options::getScreenshotDir() {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.dirMu);
    if (!s.initialized) return "";
    if (!s.screenshotDir.empty() && isDir(s.screenshotDir)) return absPath(s.screenshotDir);
    s.screenshotDir = absPath(joinPath(s.paths.dataDir, "Screenshots"));
    return s.screenshotDir;
}

std::string Options::getReplayDir() {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.dirMu);
    if (!s.initialized) return "";
    if (!s.replayDir.empty() && isDir(s.replayDir)) return absPath(s.replayDir);
    s.replayDir = absPath(joinPath(s.paths.dataDir, "Replays"));
    return s.replayDir;
}

std::string Options::getSkinRootDir() {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.dirMu);
    if (!s.initialized) return "";
    if (!s.skinRootDir.empty() && isDir(s.skinRootDir)) return absPath(s.skinRootDir);
    s.skinRootDir = absPath(joinPath(s.paths.dataDir, "Skins"));
    ensureDir(s.skinRootDir, "skins");
    return s.skinRootDir;
}

std::string Options::getSkinDir() {
    std::string root = getSkinRootDir();
    if (root.empty()) return "";
    std::string dir = absPath(joinPath(root, getSkinName()));
    return isDir(dir) ? dir : "";
}

std::string Options::getDownloadDir() {
    bool init = false;
    OptionsPaths p = pathsSnapshot(&init);
    // the platform launcher may know the real folder
    if (isDir(p.downloadDir)) return absPath(p.downloadDir);
    // desktop: the user's Downloads folder (a relative "Download" would be inside the game's folder)
    if (isDesktopPlatform()) {
        const char* home = std::getenv("HOME");
#if defined(_WIN32)
        if (!home) home = std::getenv("USERPROFILE");
#endif
        if (home) {
            std::string d = joinPath(home, "Downloads");
            if (isDir(d)) return absPath(d);
        }
    }
    return "Download";  // Java: new File("Download") (relative; it may not exist)
}

bool Options::getThemeSong(ThemeSong& out) {
    State& s = st();
    std::string theme, timing;
    {
        std::lock_guard<std::mutex> lk(s.mu);
        theme = s.themeString;
        timing = s.themeTimingPoint;
    }
    std::string audio;
    if (!themeStringValid(theme, timing, &audio)) return false;
    std::vector<std::string> tokens = java::split(theme, ',');
    out.audioFilename = audio;
    out.title = tokens[1];
    out.artist = tokens[2];
    out.endTime = java::parseInt(tokens[3]);
    out.timingPoint = timing;
    return true;
}

std::string Options::getFFmpegLocation() {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.dirMu);
    return s.ffmpegPath.empty() ? "" : absPath(s.ffmpegPath);
}

// =====================================================================================================================
// opsu.cfg
// =====================================================================================================================

namespace {

const char* const SLIDER_STYLE_KEY = "SliderStyleVersion";
const char* const SORT_ORDER_KEY = "SongSortOrder";
constexpr int SLIDER_STYLE_VERSION = 1;

// BufferedReader.readLine(): "\n", "\r\n" and a lone "\r" end a line; a last line without terminator counts.
std::vector<std::string> readLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t i = 0, n = text.size();
    while (i < n) {
        size_t j = i;
        while (j < n && text[j] != '\n' && text[j] != '\r') j++;
        lines.push_back(text.substr(i, j - i));
        if (j < n && text[j] == '\r' && j + 1 < n && text[j + 1] == '\n') j++;
        i = j + 1;
    }
    return lines;
}

// new SimpleDateFormat("EEEE, MMMM dd, yyyy").format(new Date()) in English, default time zone.
std::string headerDate(int64_t ms) {
    std::time_t t = static_cast<std::time_t>(ms / 1000);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    static const char* const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char* const months[] = {"January", "February", "March", "April", "May", "June", "July",
                                         "August", "September", "October", "November", "December"};
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d", tmv.tm_mday);
    return std::string(days[tmv.tm_wday]) + ", " + months[tmv.tm_mon] + " " + buf + ", " + std::to_string(tmv.tm_year + 1900);
}

void syncAndClose(std::FILE* f) {
    std::fflush(f);
#if defined(_WIN32)
    _commit(_fileno(f));
#else
    ::fsync(::fileno(f));  // make sure the bytes are on disk before we replace the old file
#endif
    std::fclose(f);
}

}  // namespace

int Options::getSliderStyleVersion() { return st().sliderStyleVersion.load(); }

void Options::parseOptions() {
    State& s = st();
    if (!isInitialized()) return;  // no I/O before init()
    const std::string file = getOptionsFile();
    std::error_code ec;
    // if no config file, use default settings
    if (!fs::is_regular_file(fs::path(file), ec)) {
        saveOptions();
        return;
    }
    // option map: display name -> option (Java HashMap: a later duplicate would win; there are none)
    static const std::vector<std::pair<std::string, GameOption>> kMap = [] {
        std::vector<std::pair<std::string, GameOption>> m;
        for (const GameOption& o : GameOption::values()) m.emplace_back(o.getDisplayName(), o);
        return m;
    }();
    // read file (UTF-8; malformed bytes become U+FFFD like the JDK decoder)
    std::string content;
    {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            Log::error("Failed to read file '" + file + "'.");
            return;
        }
        content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    content = java::decodeUtf8Lossy(content);
    for (const std::string& raw : readLines(content)) {
        std::string line = java::trim(raw);
        if (java::utf16Length(line) < 2 || line[0] == '#') continue;
        size_t index = line.find('=');
        if (index == std::string::npos) continue;
        // read option
        std::string name = java::trim(line.substr(0, index));
        if (name == SLIDER_STYLE_KEY) {
            try { s.sliderStyleVersion = java::parseInt(java::trim(line.substr(index + 1))); }
            catch (const java::NumberFormatException&) { s.sliderStyleVersion = 0; }
            continue;
        }
        if (name == SORT_ORDER_KEY) {
            std::lock_guard<std::mutex> lk(s.mu);
            s.savedSortOrder = java::trim(line.substr(index + 1));
            continue;
        }
        for (const auto& kv : kMap) {
            if (kv.first != name) continue;
            try {
                kv.second.read(java::trim(line.substr(index + 1)));
            } catch (const java::NumberFormatException& e) {
                Log::warn("Format error in options file for line: '" + line + "'.", e);
            } catch (const std::exception& e) {
                // one bad value must never stop the app from starting
                Log::warn("Could not read option from line: '" + line + "'.", e);
            }
            break;
        }
    }
    s.dirty = false;  // values just loaded from disk are not unsaved changes
    // An opsu.cfg written by an older build can still carry the slider settings of the old multi-style renderer.
    // Reset them once to the defaults of the single slider style, then remember that we did.
    if (s.sliderStyleVersion.load() < SLIDER_STYLE_VERSION) {
        GameOption::SNAKING_SLIDERS.read("true");
        GameOption::SLIDER_SHRINK.read("true");
        GameOption::SLIDER_MERGE.read("true");
        GameOption::SLIDER_CAPS.read("false");
        s.sliderStyleVersion = SLIDER_STYLE_VERSION;
        saveOptions();
    }
}

void Options::flushIfDue() {
    State& s = st();
    if (s.dirty.load() && nowMs() - s.dirtySince.load() >= SAVE_DEBOUNCE_MS) saveOptions();
}

void Options::flushNow() {
    if (st().dirty.load()) saveOptions();
}

void Options::saveOptions() {
    State& s = st();
    std::lock_guard<std::mutex> saveLock(s.saveMu);
    s.dirty = false;
    if (!isInitialized()) return;  // no I/O before init()
    const std::string dst = getOptionsFile();
    const std::string tmp = dst + ".tmp";
    // Build the text first (the write() of the directory options can create folders).
    std::string out;
    out += std::string("# ") + OpsuConstants::PROJECT_NAME + " configuration\n";
    out += "# last updated on " + headerDate(nowMs()) + "\n";
    out += "\n";
    out += std::string(SLIDER_STYLE_KEY) + " = " + std::to_string(SLIDER_STYLE_VERSION) + "\n";
    {
        std::string sort = getSavedSortOrder();
        if (!sort.empty()) out += std::string(SORT_ORDER_KEY) + " = " + sort + "\n";
    }
    for (const GameOption& o : GameOption::values()) {
        if (o == GameOption::NULL_OPTION) continue;
        out += o.getDisplayName() + " = " + o.write() + "\n";
    }
    // Write to a temp file and rename it over the real one: a crash or kill mid-write can no longer leave a truncated opsu.cfg.
    std::error_code ec;
    fs::create_directories(fs::path(dst).parent_path(), ec);  // (the Java relies on the folder existing)
    bool written = false;
    if (std::FILE* f = std::fopen(tmp.c_str(), "wb")) {
        written = std::fwrite(out.data(), 1, out.size(), f) == out.size();
        syncAndClose(f);
    }
    if (!written) {
        Log::error("Failed to write to file '" + dst + "'.");
        return;
    }
    fs::rename(fs::path(tmp), fs::path(dst), ec);
    if (ec) {
        fs::remove(fs::path(dst), ec);
        std::error_code ec2;
        fs::rename(fs::path(tmp), fs::path(dst), ec2);
        if (ec2) Log::error("Failed to replace file '" + dst + "'.");
    }
}

}  // namespace opsu
