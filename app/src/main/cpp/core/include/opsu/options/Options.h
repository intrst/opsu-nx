#pragma once
// Port of itdelatrisu.opsu.options.Options (milestone 1: GameOption enum, defaults, accessors, seams).
// See docs/agents/T10.md for the WIP list, the seam list and the deviations from the Java.
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "opsu/Resolution.h"

namespace opsu {

// ---------------------------------------------------------------------------------------------------------------------
// Key codes the Options code compares against. libGDX numbering (same as the Android key codes; see
// platform/input/AndroidInput.cpp and opsu/input/Keys.h). Java: Input.KEY_*, Input.ANDROID_BACK, Input.MOUSE_BACK_BUTTON.
// ---------------------------------------------------------------------------------------------------------------------
namespace OptionKeys {
inline constexpr int KEY_NONE = 0;  // Keyboard.KEY_NONE
inline constexpr int KEY_Z = 54, KEY_X = 52;
inline constexpr int ANDROID_BACK = 4;       // Keys.BACK
inline constexpr int MOUSE_BACK_BUTTON = 3;  // Buttons.BACK
inline constexpr int KEY_ESCAPE = 111, KEY_SPACE = 62, KEY_UP = 19, KEY_DOWN = 20;
inline constexpr int KEY_F7 = 137, KEY_F10 = 140, KEY_F12 = 142;  // libGDX >= 1.9.13: F1 == 131
}  // namespace OptionKeys

// ---------------------------------------------------------------------------------------------------------------------
// Seams for classes that are not ported to C++ yet. Every field has a harmless default (empty std::function = no-op, or
// the value stated in the comment). Set them once at start-up with Options::setSeams(), before other threads read options.
// ---------------------------------------------------------------------------------------------------------------------

/** The two GameContainer calls Options makes (Java: GameContainer.setTargetFrameRate / setVSync). */
class IGameContainer {
public:
    virtual ~IGameContainer() = default;
    virtual void setTargetFrameRate(int fps) = 0;
    virtual void setVSync(bool vsync) = 0;
};

/** What Options.setDisplayMode(Container) needs (Java: itdelatrisu.opsu.Container). Desktop only. */
class IDisplayHost {
public:
    virtual ~IDisplayHost() = default;
    virtual int getScreenWidth() = 0;
    virtual int getScreenHeight() = 0;
    virtual void setDisplayMode(int width, int height, bool fullscreen) = 0;
};

/**
 * The parsed theme song (Java Options.getThemeBeatmap() builds a dummy Beatmap from it; the Beatmap itself is left to the
 * caller, see "seams" in docs/agents/T10.md). timingPoint is the raw "offset,msPerBeat,..." line (it parses as a TimingPoint).
 */
struct ThemeSong {
    std::string audioFilename, title, artist;
    int endTime = 0;  // ms
    std::string timingPoint;
};

/**
 * Base folders, injected once with Options::init(). Java derives them from "./opsu" (fake/File.java resolves that against the
 * shared storage on Android); the port never calls platform APIs, main.cpp fills this in (e.g. "/storage/emulated/0/opsu").
 * The default constructor gives desktop-test values under the system temp directory (it only reads the temp path, no file I/O).
 */
struct OptionsPaths {
    std::string configDir;    // opsu.cfg, opsu.log   (Java CONFIG_DIR)
    std::string dataDir;      // Songs/ Skins/ Import/ Replays/ Screenshots/, opsu.db, opsu_scores.db   (Java DATA_DIR)
    std::string cacheDir;     // Natives/, Temp/   (Java CACHE_DIR)
    std::string localDir;     // app-private storage: where older builds kept the DBs   (Java File.local(...))
    std::string downloadDir;  // the device's public Download folder   (Java DeviceInfo.info.getDownloadDir())
    OptionsPaths();
};

struct OptionsSeams {
    /** UI.getNotificationManager().sendBarNotification(msg). Default: no-op. */
    std::function<void(const std::string&)> sendBarNotification;
    /** UI.resetFPSDisplay(). Default: no-op. */
    std::function<void()> resetFPSDisplay;
    /** Fonts.LARGE/MEDIUM/DEFAULT.loadGlyphs() (SHOW_UNICODE turned on). Default: no-op. */
    std::function<void()> loadUnicodeGlyphs;
    /** SoundStore.get().setMusicVolume(v) + MusicController.onVolumeAdjusted(); v = master * music. Default: no-op. */
    std::function<void(float)> onVolumeAdjusted;
    /** MusicController.setVolume(v) (called by setMasterVolume); v = master * music. Default: no-op. */
    std::function<void(float)> setMusicControllerVolume;
    /** SkinSwitcher.request(name). Default: no-op. */
    std::function<void(const std::string&)> requestSkin;
    /** SkinLoader.getSkinDirectories(root): the names (not paths) of the skin sub-directories. Default: empty list. */
    std::function<std::vector<std::string>(const std::string& rootDir)> listSkinDirectories;
    /** Gdx.graphics.getWidth() / getPpiX(): screen width in inches. Default: 10 (a desktop-sized screen). */
    std::function<float()> screenWidthInches;
    /** Gdx.app.getType() == Desktop. Default: false (the port targets Android). */
    std::function<bool()> isDesktop;
    /** Gdx.app != null && type is Android or iOS. Default: false (set it to true in main.cpp on Android). */
    std::function<bool()> isTouchPlatform;
    /** Display.getDesktopDisplayMode() (width, height). Default: (0, 0), so only 800x600 is listed. */
    std::function<std::pair<int, int>()> desktopDisplaySize;
    /** Keyboard.getKeyName(code) (libGDX Keys.toString). Default: Options::defaultKeyName (A-Z, 0-9, F1-F12 only). */
    std::function<std::string(int)> keyName;
    /** Keyboard.getKeyIndex(name) (libGDX Keys.valueOf, -1 if unknown). Default: Options::defaultKeyIndex. */
    std::function<int(const std::string&)> keyIndex;
    /** ResourceLoader.resourceExists(name) for a bundled asset (the theme song). Default: false. */
    std::function<bool(const std::string&)> bundledResourceExists;
    /** System.currentTimeMillis() for the save debounce. Default: system_clock. */
    std::function<int64_t()> nowMs;
};

// ---------------------------------------------------------------------------------------------------------------------
// GameOption: Java's Options.GameOption enum. A small handle (the ordinal); the values live in the Options state, so
// GameOption::MASTER_VOLUME.getIntegerValue() reads like the Java. Order = order of the Java enum = order in opsu.cfg.
// ---------------------------------------------------------------------------------------------------------------------
class GameOption {
public:
    enum class Id : int {
        // NULL (Java) is renamed: NULL is a macro in C++.
        NULL_OPTION,
        BEATMAP_DIRECTORY,
        IMPORT_DIRECTORY,
        SCREENSHOT_DIRECTORY,
        REPLAY_DIRECTORY,
        SKIN_DIRECTORY,
        FFMPEG_PATH,
        THEME_SONG,
        THEME_SONG_TIMINGPOINT,
        SCREEN_RESOLUTION,
        FULLSCREEN,
        SKIN,
        TARGET_FPS,
        SHOW_FPS,
        SHOW_UNICODE,
        SCREENSHOT_FORMAT,
        CURSOR_SIZE,
        DYNAMIC_BACKGROUND,
        SHOW_INTRO,
        LOAD_VERBOSE,
        MASTER_VOLUME,
        MUSIC_VOLUME,
        EFFECT_VOLUME,
        HITSOUND_VOLUME,
        MUSIC_OFFSET,
        RENDER_OFFSET,
        DISABLE_GAMEPLAY_SOUNDS,
        DISABLE_SOUNDS,
        KEY_LEFT,
        KEY_RIGHT,
        DISABLE_MOUSE_WHEEL,
        DISABLE_MOUSE_BUTTONS,
        MULTITOUCH_GAMEPLAY,
        TOUCH_MAX_FINGERS,
        TOUCH_PALM_REJECTION,
        DISABLE_CURSOR,
        BACKGROUND_DIM,
        BACKGROUND_BLUR,
        FORCE_DEFAULT_PLAYFIELD,
        ENABLE_STORYBOARD,
        ENABLE_VIDEOS,
        IGNORE_BEATMAP_SKINS,
        FORCE_SKIN_CURSOR,
        SNAKING_SLIDERS,
        SLIDER_CAPS,
        SLIDER_SHRINK,
        SLIDER_MERGE,
        SHOW_HIT_LIGHTING,
        SHOW_HIT_STARS,
        SHOW_COMBO_BURSTS,
        SHOW_PERFECT_HIT,
        SHOW_FOLLOW_POINTS,
        SHOW_HIT_ERROR_BAR,
        ALWAYS_SHOW_KEY_OVERLAY,
        LOAD_HD_IMAGES,
        FIXED_CS,
        FIXED_HP,
        FIXED_AR,
        FIXED_OD,
        FIXED_SPEED,
        CHECKPOINT,
        SONG_THUMBNAILS,
        PARALLAX,
        ENABLE_THEME_SONG,
        REPLAY_SEEKING,
        DISABLE_UPDATER,
        ENABLE_WATCH_SERVICE,
        IN_GAME_PAUSE,
        PLAYER_LOADER,
        MOBILE_UI_SCALING,
        PLAYFIELDSCALE,
        SCOREBOARD,
        AVGOFFSETDISP,
        DISABLE_SPINNER_UI,
        DISABLE_SPINNER_SOUND,
        COUNT_  // number of options
    };
    enum class OptionType { BOOLEAN, NUMERIC, SELECT };

    static constexpr int COUNT = static_cast<int>(Id::COUNT_);

    constexpr explicit GameOption(Id id) : id_(id) {}

    // one static instance per Java enum constant (defined below the class)
    static const GameOption NULL_OPTION, BEATMAP_DIRECTORY, IMPORT_DIRECTORY, SCREENSHOT_DIRECTORY, REPLAY_DIRECTORY,
        SKIN_DIRECTORY, FFMPEG_PATH, THEME_SONG, THEME_SONG_TIMINGPOINT, SCREEN_RESOLUTION, FULLSCREEN, SKIN, TARGET_FPS,
        SHOW_FPS, SHOW_UNICODE, SCREENSHOT_FORMAT, CURSOR_SIZE, DYNAMIC_BACKGROUND, SHOW_INTRO, LOAD_VERBOSE, MASTER_VOLUME,
        MUSIC_VOLUME, EFFECT_VOLUME, HITSOUND_VOLUME, MUSIC_OFFSET, RENDER_OFFSET, DISABLE_GAMEPLAY_SOUNDS, DISABLE_SOUNDS,
        KEY_LEFT, KEY_RIGHT, DISABLE_MOUSE_WHEEL, DISABLE_MOUSE_BUTTONS, MULTITOUCH_GAMEPLAY, TOUCH_MAX_FINGERS,
        TOUCH_PALM_REJECTION, DISABLE_CURSOR, BACKGROUND_DIM, BACKGROUND_BLUR, FORCE_DEFAULT_PLAYFIELD, ENABLE_STORYBOARD,
        ENABLE_VIDEOS, IGNORE_BEATMAP_SKINS, FORCE_SKIN_CURSOR, SNAKING_SLIDERS, SLIDER_CAPS, SLIDER_SHRINK, SLIDER_MERGE,
        SHOW_HIT_LIGHTING, SHOW_HIT_STARS, SHOW_COMBO_BURSTS, SHOW_PERFECT_HIT, SHOW_FOLLOW_POINTS, SHOW_HIT_ERROR_BAR,
        ALWAYS_SHOW_KEY_OVERLAY, LOAD_HD_IMAGES, FIXED_CS, FIXED_HP, FIXED_AR, FIXED_OD, FIXED_SPEED, CHECKPOINT,
        SONG_THUMBNAILS, PARALLAX, ENABLE_THEME_SONG, REPLAY_SEEKING, DISABLE_UPDATER, ENABLE_WATCH_SERVICE, IN_GAME_PAUSE,
        PLAYER_LOADER, MOBILE_UI_SCALING, PLAYFIELDSCALE, SCOREBOARD, AVGOFFSETDISP, DISABLE_SPINNER_UI,
        DISABLE_SPINNER_SOUND;

    /** GameOption.values(): all options in declaration order (including NULL_OPTION). */
    static const std::array<GameOption, COUNT>& values();
    /** Enum.valueOf-like lookup by Java constant name; returns false if unknown. */
    static bool fromConstantName(const std::string& constantName, GameOption& out);

    Id id() const { return id_; }
    int ordinal() const { return static_cast<int>(id_); }
    /** The Java constant name (e.g. "MASTER_VOLUME"; "NULL" for NULL_OPTION). */
    const char* constantName() const;
    bool operator==(const GameOption& o) const { return id_ == o.id_; }
    bool operator!=(const GameOption& o) const { return id_ != o.id_; }

    /** Option name (empty for internal options; the Java has null there). */
    const std::string& getName() const;
    /** Option name as written in opsu.cfg. */
    const std::string& getDisplayName() const;
    /** Option description (empty for internal options; the Java has null there). */
    const std::string& getDescription() const;
    OptionType getType() const;
    bool isRestartRequired() const;

    bool getBooleanValue() const;
    int getIntegerValue() const;
    int getDefaultIntegerValue() const;
    bool isCustomized() const;
    /** Numeric options only: setValue(default) (so volume options apply immediately). */
    void resetToDefault() const;
    int getMinValue() const;
    int getMaxValue() const;
    /** The choices of a SELECT option (SCREEN_RESOLUTION, SKIN, TARGET_FPS, SCREENSHOT_FORMAT); empty = Java null. */
    std::vector<std::string> getItemList() const;

    void setValue(bool value) const;  // marks the options dirty
    /** Clamped to [min, max] like Utils.clamp; marks the options dirty. MASTER/MUSIC_VOLUME also fire the volume seam. */
    void setValue(int value) const;
    /** Java toggle(container): container may be null. Note: like the Java it does NOT mark the options dirty. */
    void toggle(IGameContainer* container = nullptr) const;
    void selectItem(int index, IGameContainer* container = nullptr) const;
    void click(IGameContainer*) const {}

    std::string getValueString() const;
    /** The string written to opsu.cfg. */
    std::string write() const;
    /** Reads a value from opsu.cfg. Throws java::NumberFormatException like the Java for bad numbers. */
    void read(const std::string& s) const;
    /** Java matches(): case-insensitive (ASCII) contains on name or description. query must be lower case. */
    bool matches(const std::string& query) const;

    void setVisible(bool visible) const;
    bool isVisible() const;

private:
    Id id_;
};

#define OPSU_GAMEOPTION_DEF(N) inline const GameOption GameOption::N{GameOption::Id::N}
OPSU_GAMEOPTION_DEF(NULL_OPTION);
OPSU_GAMEOPTION_DEF(BEATMAP_DIRECTORY);
OPSU_GAMEOPTION_DEF(IMPORT_DIRECTORY);
OPSU_GAMEOPTION_DEF(SCREENSHOT_DIRECTORY);
OPSU_GAMEOPTION_DEF(REPLAY_DIRECTORY);
OPSU_GAMEOPTION_DEF(SKIN_DIRECTORY);
OPSU_GAMEOPTION_DEF(FFMPEG_PATH);
OPSU_GAMEOPTION_DEF(THEME_SONG);
OPSU_GAMEOPTION_DEF(THEME_SONG_TIMINGPOINT);
OPSU_GAMEOPTION_DEF(SCREEN_RESOLUTION);
OPSU_GAMEOPTION_DEF(FULLSCREEN);
OPSU_GAMEOPTION_DEF(SKIN);
OPSU_GAMEOPTION_DEF(TARGET_FPS);
OPSU_GAMEOPTION_DEF(SHOW_FPS);
OPSU_GAMEOPTION_DEF(SHOW_UNICODE);
OPSU_GAMEOPTION_DEF(SCREENSHOT_FORMAT);
OPSU_GAMEOPTION_DEF(CURSOR_SIZE);
OPSU_GAMEOPTION_DEF(DYNAMIC_BACKGROUND);
OPSU_GAMEOPTION_DEF(SHOW_INTRO);
OPSU_GAMEOPTION_DEF(LOAD_VERBOSE);
OPSU_GAMEOPTION_DEF(MASTER_VOLUME);
OPSU_GAMEOPTION_DEF(MUSIC_VOLUME);
OPSU_GAMEOPTION_DEF(EFFECT_VOLUME);
OPSU_GAMEOPTION_DEF(HITSOUND_VOLUME);
OPSU_GAMEOPTION_DEF(MUSIC_OFFSET);
OPSU_GAMEOPTION_DEF(RENDER_OFFSET);
OPSU_GAMEOPTION_DEF(DISABLE_GAMEPLAY_SOUNDS);
OPSU_GAMEOPTION_DEF(DISABLE_SOUNDS);
OPSU_GAMEOPTION_DEF(KEY_LEFT);
OPSU_GAMEOPTION_DEF(KEY_RIGHT);
OPSU_GAMEOPTION_DEF(DISABLE_MOUSE_WHEEL);
OPSU_GAMEOPTION_DEF(DISABLE_MOUSE_BUTTONS);
OPSU_GAMEOPTION_DEF(MULTITOUCH_GAMEPLAY);
OPSU_GAMEOPTION_DEF(TOUCH_MAX_FINGERS);
OPSU_GAMEOPTION_DEF(TOUCH_PALM_REJECTION);
OPSU_GAMEOPTION_DEF(DISABLE_CURSOR);
OPSU_GAMEOPTION_DEF(BACKGROUND_DIM);
OPSU_GAMEOPTION_DEF(BACKGROUND_BLUR);
OPSU_GAMEOPTION_DEF(FORCE_DEFAULT_PLAYFIELD);
OPSU_GAMEOPTION_DEF(ENABLE_STORYBOARD);
OPSU_GAMEOPTION_DEF(ENABLE_VIDEOS);
OPSU_GAMEOPTION_DEF(IGNORE_BEATMAP_SKINS);
OPSU_GAMEOPTION_DEF(FORCE_SKIN_CURSOR);
OPSU_GAMEOPTION_DEF(SNAKING_SLIDERS);
OPSU_GAMEOPTION_DEF(SLIDER_CAPS);
OPSU_GAMEOPTION_DEF(SLIDER_SHRINK);
OPSU_GAMEOPTION_DEF(SLIDER_MERGE);
OPSU_GAMEOPTION_DEF(SHOW_HIT_LIGHTING);
OPSU_GAMEOPTION_DEF(SHOW_HIT_STARS);
OPSU_GAMEOPTION_DEF(SHOW_COMBO_BURSTS);
OPSU_GAMEOPTION_DEF(SHOW_PERFECT_HIT);
OPSU_GAMEOPTION_DEF(SHOW_FOLLOW_POINTS);
OPSU_GAMEOPTION_DEF(SHOW_HIT_ERROR_BAR);
OPSU_GAMEOPTION_DEF(ALWAYS_SHOW_KEY_OVERLAY);
OPSU_GAMEOPTION_DEF(LOAD_HD_IMAGES);
OPSU_GAMEOPTION_DEF(FIXED_CS);
OPSU_GAMEOPTION_DEF(FIXED_HP);
OPSU_GAMEOPTION_DEF(FIXED_AR);
OPSU_GAMEOPTION_DEF(FIXED_OD);
OPSU_GAMEOPTION_DEF(FIXED_SPEED);
OPSU_GAMEOPTION_DEF(CHECKPOINT);
OPSU_GAMEOPTION_DEF(SONG_THUMBNAILS);
OPSU_GAMEOPTION_DEF(PARALLAX);
OPSU_GAMEOPTION_DEF(ENABLE_THEME_SONG);
OPSU_GAMEOPTION_DEF(REPLAY_SEEKING);
OPSU_GAMEOPTION_DEF(DISABLE_UPDATER);
OPSU_GAMEOPTION_DEF(ENABLE_WATCH_SERVICE);
OPSU_GAMEOPTION_DEF(IN_GAME_PAUSE);
OPSU_GAMEOPTION_DEF(PLAYER_LOADER);
OPSU_GAMEOPTION_DEF(MOBILE_UI_SCALING);
OPSU_GAMEOPTION_DEF(PLAYFIELDSCALE);
OPSU_GAMEOPTION_DEF(SCOREBOARD);
OPSU_GAMEOPTION_DEF(AVGOFFSETDISP);
OPSU_GAMEOPTION_DEF(DISABLE_SPINNER_UI);
OPSU_GAMEOPTION_DEF(DISABLE_SPINNER_SOUND);
#undef OPSU_GAMEOPTION_DEF

// ---------------------------------------------------------------------------------------------------------------------
// Options: the static facade. All getters are safe to call from any thread (atomics / mutex inside).
// ---------------------------------------------------------------------------------------------------------------------
class Options {
public:
    using GameOption = opsu::GameOption;

    // constants (Java: public static final)
    static constexpr const char* FONT_MAIN = "Exo2-Regular.ttf";
    static constexpr const char* FONT_BOLD = "Exo2-Bold.ttf";
    static constexpr const char* FONT_CJK = "DroidSansFallback.ttf";
    static constexpr const char* VERSION_FILE = "version";
    static constexpr const char* USER_AGENT =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/63.0.3239.132 Safari/537.36";
    static constexpr const char* DEFAULT_SKIN_NAME = "Default";  // Skin.DEFAULT_SKIN_NAME

    /** Installs the seams (see OptionsSeams) and re-resolves the two screen-size dependent defaults. */
    static void setSeams(OptionsSeams seams);
    /** All options back to the Java defaults, seams back to the defaults, dirty flag cleared. */
    static void resetForTests();

    // ---- accessors (same names as the Java) ----
    static int getTargetFPS();
    static void setNextFPS(IGameContainer* container);
    static float getMasterVolume();
    static void setMasterVolume(IGameContainer* container, float volume);
    static float getMusicVolume();
    static float getEffectVolume();
    static float getHitSoundVolume();
    static int getMusicOffset();
    static int getRenderOffset();
    static std::string getScreenshotFormat();
    static void setDisplayMode(IDisplayHost& app);
    /** The selected window size (Options.Resolution). */
    static Resolution getResolution();
    static bool isFullscreen();
    static bool isFPSCounterEnabled();
    static void toggleFPSCounter();
    static bool isHitLightingEnabled();
    static bool isHitStarsEnabled();
    static bool isComboBurstEnabled();
    static float getCursorScale();
    static bool isDynamicBackgroundEnabled();
    static bool isPerfectHitBurstEnabled();
    static bool isFollowPointEnabled();
    static float getBackgroundDim();
    static int getBackgroundBlur();
    static bool isDefaultPlayfieldForced();
    static bool isStoryboardEnabled();
    static bool isBeatmapVideoEnabled();
    static bool isBeatmapSkinIgnored();
    static bool isSkinCursorForced();
    static bool isSliderSnaking();
    static bool isSliderCapsDrawn();
    static bool isSliderShrinking();
    static bool isSliderMerging();
    static float getFixedCS();
    static float getFixedHP();
    static float getFixedAR();
    static float getFixedOD();
    static float getFixedSpeed();
    static bool isLoadVerbose();
    static bool isIntroEnabled();
    static int getCheckpoint();
    static bool isGameplaySoundDisabled();
    static bool isSoundDisabled();
    static bool useUnicodeMetadata();
    static bool isSongThumbnailsEnabled();
    static bool isParallaxEnabled();
    static bool isThemeSongEnabled();
    static bool isReplaySeekingEnabled();
    static bool isUpdaterDisabled();
    static bool isWatchServiceEnabled();
    static bool setCheckpoint(int time);
    static bool isHitErrorBarEnabled();
    static bool alwaysShowKeyOverlay();
    static bool loadHDImages();
    static bool isMouseWheelDisabled();
    static bool isMouseDisabled();
    static bool isMultitouchGameplay();
    /** Gdx.app.getType() == Desktop, from the isDesktop seam (used by OptionGroup). */
    static bool isDesktopPlatform();
    static bool isTouchPalmRejection();
    static int getMaxTouchFingers();
    static void toggleMouseDisabled();
    static bool isCursorDisabled();
    static int getGameKeyLeft();
    static int getGameKeyRight();
    static bool setGameKeyLeft(int key);
    static bool setGameKeyRight(int key);
    static float getMobileUIScale();
    static float getMobileUIScale(float scale);
    static float getMobileUIScaleHigh();
    static float getPlayfieldScale();
    static bool isInGamePauseEnabled();
    static bool isPlayerLoaderEnabled();
    static std::string getSkinName();
    static void setSkinName(const std::string& name);
    static std::string getSavedSortOrder();  // "" = none saved (Java: null)
    static void setSavedSortOrder(const std::string& name);

    // ---- dirty tracking and saving ----
    static void markDirty();
    static bool isDirty();
    static int64_t getDirtySinceMs();
    static constexpr int64_t SAVE_DEBOUNCE_MS = 400;
    /** Writes the options if something changed and SAVE_DEBOUNCE_MS passed since the last change. Call once per frame. */
    static void flushIfDue();
    /** Writes the options immediately if something changed (pause, back-navigation, exit). */
    static void flushNow();
    /**
     * Reads opsu.cfg ("Key = Value" lines; '#' comments; unknown keys ignored; bad numbers logged and skipped). If the file does
     * not exist the defaults are written instead. Does nothing before init().
     */
    static void parseOptions();
    /** (Over)writes opsu.cfg through a temp file + rename. Same order and number formats as the Java. Does nothing before init(). */
    static void saveOptions();
    /** The "SliderStyleVersion" read from opsu.cfg (0 if absent). */
    static int getSliderStyleVersion();

    // ---- directories and files (all return absolute paths with '/'; "" before init()) ----
    /** Whether to use XDG directories (Java: always false in this port). */
    static constexpr bool USE_XDG = false;
    /**
     * Sets the base folders. Does NO file I/O (directories are created lazily by the getters below, like the Java) and forgets
     * any directory read from opsu.cfg earlier, so call it before parseOptions(). Call once at start-up.
     */
    static void init(const OptionsPaths& paths);
    static bool isInitialized();
    static std::string getConfigDir();
    static std::string getDataDir();
    static std::string getCacheDir();
    static std::string getLogFile();         // <config>/opsu.log
    static std::string getOptionsFile();     // <config>/opsu.cfg
    static std::string getOldOptionsFile();  // <config>/../.opsu.cfg
    static std::string getBeatmapDB();       // <data>/opsu.db
    static std::string getOldBeatmapDB();    // <data>/.opsu.db
    static std::string getLocalBeatmapDB();  // <local>/opsu.db
    static std::string getScoreDB();         // <data>/opsu_scores.db
    static std::string getOldScoreDB();      // <data>/.opsu_scores.db
    static std::string getLocalScoreDB();    // <local>/opsu_scores.db
    static std::string getNativeDir();       // <cache>/Natives
    static std::string getTempDir();         // <cache>/Temp
    /** The beatmap directory; if invalid it falls back to <data>/Songs and creates it. */
    static std::string getBeatmapDir();
    /** The import directory; if invalid it becomes <data>/Import and is created. */
    static std::string getImportDir();
    /** The screenshot directory: <data>/Screenshots unless a valid one was configured (not created). */
    static std::string getScreenshotDir();
    /** The replay directory: <data>/Replays unless a valid one was configured (not created). */
    static std::string getReplayDir();
    /** The root skin directory; if invalid it falls back to <data>/Skins and creates it. */
    static std::string getSkinRootDir();
    /** The directory of the selected skin, or "" for the default skin (Java: null). */
    static std::string getSkinDir();
    /** The device's public Download folder (scanned for .osz/.osk at start-up). */
    static std::string getDownloadDir();
    /** The custom FFmpeg library location, or "" for the default (Java: null). */
    static std::string getFFmpegLocation();
    /**
     * The theme song of Options.getThemeBeatmap(): false where the Java returns null (theme string not "file,title,artist,lengthMs"
     * with an integer length, or an unparsable timing point). Does not build a Beatmap.
     */
    static bool getThemeSong(ThemeSong& out);

    // ---- key names (libGDX Keys.toString / valueOf) ----
    static std::string keyName(int keycode);   // "Null" for keycode <= 0, like Keyboard.getKeyName
    static int keyIndex(const std::string& name);
    static std::string defaultKeyName(int keycode);
    static int defaultKeyIndex(const std::string& name);

private:
    Options() = delete;
};

}  // namespace opsu
