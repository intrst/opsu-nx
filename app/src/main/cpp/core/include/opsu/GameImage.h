#pragma once
// Port of itdelatrisu/opsu/GameImage.java (T11): the registry of every game image and how each one is found
// (beatmap skin, game skin, built-in res/ default), loaded, scaled and animated.
//
// Java -> C++:  `GameImage.CURSOR.getImage()`  ->  `gameimage::getImage(GameImage::CURSOR)`.
// An enum class cannot have methods, so the Java instance methods are free functions in namespace opsu::gameimage that
// take the entry; the static Java methods (init, getUIscale, clearReferences, ...) are free functions there too.
// The entry table (names, patterns, flags, processing kind) is GENERATED from the Java source into
// src/GameImageTable.cpp by docs/agents/T11_tools/gen_gameimage_table.py; the enum list below is regenerated too.
//
// NOT THREAD SAFE: render thread only (the state is one global registry, like the Java enum's fields).
//
// Seams for what is not ported yet (all have harmless defaults so tests run): GameImageHost (Options, Fonts, SkinCache,
// the bundled res/ files) and SkinInfo (Skin). See the comments on both. Images are shared_ptr<Image>: the Java hands the
// same Image object to many callers, and an Animation holds the very same frames as the GameImage.
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opsu/gfx/Animation.h"
#include "opsu/gfx/Image.h"
#include "opsu/util/Color.h"

namespace opsu {

/** All game images, in the order of the Java enum (the order is the ordinal). */
enum class GameImage : int {
    // BEGIN GENERATED ENUM (gen_gameimage_table.py)
    CURSOR,
    CURSOR_MIDDLE,
    CURSOR_TRAIL,
    INPUTOVERLAY_KEY,
    INPUTOVERLAY_BACKGROUND,
    SECTION_PASS,
    SECTION_FAIL,
    WARNINGARROW,
    ARROW_WARNING,
    SKIP,
    COUNTDOWN_READY,
    COUNTDOWN_3,
    COUNTDOWN_2,
    COUNTDOWN_1,
    COUNTDOWN_GO,
    HITCIRCLE_SELECT,
    UNRANKED,
    FOLLOWPOINT,
    PAUSE_CONTINUE,
    PAUSE_RETRY,
    PAUSE_BACK,
    PAUSE_REPLAY,
    PAUSE_OVERLAY,
    FAIL_BACKGROUND,
    HITCIRCLE,
    HITCIRCLE_OVERLAY,
    APPROACHCIRCLE,
    SLIDER_STARTCIRCLE,
    SLIDER_STARTCIRCLE_OVERLAY,
    SLIDER_BALL,
    SLIDER_FOLLOWCIRCLE,
    REVERSEARROW,
    SLIDER_TICK,
    SPINNER_BACKGROUND,
    SPINNER_CIRCLE,
    SPINNER_APPROACHCIRCLE,
    SPINNER_METRE,
    SPINNER_GLOW,
    SPINNER_BOTTOM,
    SPINNER_TOP,
    SPINNER_MIDDLE,
    SPINNER_MIDDLE2,
    SPINNER_SPIN,
    SPINNER_CLEAR,
    SPINNER_OSU,
    SPINNER_RPM,
    COMBO_BURST,
    SCOREBAR_BG,
    SCOREBAR_COLOUR,
    SCOREBAR_MARKER,
    SCOREBAR_KI,
    SCOREBAR_KI_DANGER,
    SCOREBAR_KI_DANGER2,
    HIT_MISS,
    HIT_50,
    HIT_100,
    HIT_300,
    HIT_100K,
    HIT_300K,
    HIT_300G,
    HIT_SLIDER10,
    HIT_SLIDER30,
    RANKING_SS,
    RANKING_SS_SMALL,
    RANKING_SSH,
    RANKING_SSH_SMALL,
    RANKING_S,
    RANKING_S_SMALL,
    RANKING_SH,
    RANKING_SH_SMALL,
    RANKING_A,
    RANKING_A_SMALL,
    RANKING_B,
    RANKING_B_SMALL,
    RANKING_C,
    RANKING_C_SMALL,
    RANKING_D,
    RANKING_D_SMALL,
    RANKING_PANEL,
    RANKING_PERFECT,
    RANKING_TITLE,
    RANKING_MAXCOMBO,
    RANKING_ACCURACY,
    RANKING_GRAPH,
    DEFAULT_0,
    DEFAULT_1,
    DEFAULT_2,
    DEFAULT_3,
    DEFAULT_4,
    DEFAULT_5,
    DEFAULT_6,
    DEFAULT_7,
    DEFAULT_8,
    DEFAULT_9,
    SCORE_0,
    SCORE_1,
    SCORE_2,
    SCORE_3,
    SCORE_4,
    SCORE_5,
    SCORE_6,
    SCORE_7,
    SCORE_8,
    SCORE_9,
    SCORE_COMMA,
    SCORE_DOT,
    SCORE_PERCENT,
    SCORE_X,
    COMBO_0,
    COMBO_1,
    COMBO_2,
    COMBO_3,
    COMBO_4,
    COMBO_5,
    COMBO_6,
    COMBO_7,
    COMBO_8,
    COMBO_9,
    COMBO_X,
    LIGHTING,
    MOD_EASY,
    MOD_NO_FAIL,
    MOD_HARD_ROCK,
    MOD_SUDDEN_DEATH,
    MOD_SPUN_OUT,
    MOD_AUTO,
    MOD_HALF_TIME,
    MOD_DOUBLE_TIME,
    MOD_NIGHTCORE,
    MOD_HIDDEN,
    MOD_FLASHLIGHT,
    MOD_RELAX,
    MOD_AUTOPILOT,
    SELECTION_MODS,
    SELECTION_MODS_OVERLAY,
    SELECTION_RANDOM,
    SELECTION_RANDOM_OVERLAY,
    SELECTION_OPTIONS,
    SELECTION_OPTIONS_OVERLAY,
    SELECTION_OTHER_OPTIONS,
    SELECTION_OTHER_OPTIONS_OVERLAY,
    SELECTION_MODE,
    MODE_OSU_SMALL,
    MODE_MANIA_SMALL,
    SONGSELECT_BOTTOM,
    REPLAY_PLAYBACK_NORMAL,
    REPLAY_PLAYBACK_DOUBLE,
    REPLAY_PLAYBACK_HALF,
    VOLUME,
    MENU_BACK,
    MENU_BACK_CHEVRON,
    MENU_BACK_SLOPE,
    MENU_BUTTON_BG,
    MENU_TAB,
    MENU_MUSICNOTE,
    MENU_LOADER,
    MENU_BG,
    MENU_LOGO,
    MENU_PLAY,
    MENU_EXIT,
    MENU_BUTTON_MID,
    MENU_BUTTON_LEFT,
    MENU_BUTTON_RIGHT,
    STAR,
    STAR3,
    STAR2,
    HIT_STAR,
    MUSIC_PLAY,
    MUSIC_PAUSE,
    MUSIC_NEXT,
    MUSIC_PREVIOUS,
    MUSIC_NOW_PLAYING,
    DOWNLOADS,
    SEARCH_BG,
    SEARCH_BG1,
    SEARCH_BG2,
    SEARCH_BG3,
    ICONSTAR,
    DELETE,
    HISTORY,
    REPOSITORY,
    DOWNLOAD,
    UPDATE,
    CHEVRON_DOWN,
    CHEVRON_RIGHT,
    CHEVRON_LEFT,
    SEARCH,
    CONTROL_SLIDER_BALL,
    CONTROL_CHECK_ON,
    CONTROL_CHECK_OFF,
    MENU_NAV_AUDIO,
    MENU_NAV_CUSTOM,
    MENU_NAV_GAMEPLAY,
    MENU_NAV_GENERAL,
    MENU_NAV_GRAPHICS,
    MENU_NAV_INPUT,
    MENU_NAV_SKIN,
    USER,
    ALPHA_MAP,
    // END GENERATED ENUM
};

/** How an entry's `process_sub(img, w, h)` override scales the loaded image (ProcKind::None: no override). */
enum class ProcKind : unsigned char {
    None,
    ScaleMobileUI,          ///< img.getScaledCopy(Options.getMobileUIScale(p1))
    ScaleMin1pxMobileUI,    ///< scaleMin1px(img, Options.getMobileUIScale(p1))
    ScaleFixed,             ///< img.getScaledCopy(p1)
    FitHeightDiv,           ///< img.getScaledCopy((h / p1) / img.getHeight())
    FitHeightMul,           ///< img.getScaledCopy((h * p1) / img.getHeight())
    FitHeightMulMobileUI,   ///< img.getScaledCopy((h * p1) / img.getHeight() * Options.getMobileUIScale(p2))
    FitWidth,               ///< img.getScaledCopy((float) w / img.getWidth())
    StretchWH,              ///< img.getScaledCopy(w, h)
    StretchWHAlpha,         ///< img.setAlpha(p1); img.getScaledCopy(w, h)
    FitButtonBgHeight,      ///< img.getScaledCopy((MENU_BUTTON_BG.getImage().getHeight() * p1) / img.getHeight())
    MenuTab,                ///< MENU_TAB: th = h * 0.033f * mobileUIScale(0.5f), tw = th * 222f / 42f (see GameImage.cpp)
    MusicNoteFontSquare,    ///< MENU_MUSICNOTE: r = (int)((Fonts.LARGE + Fonts.DEFAULT line heights - 8) / uiscale); scaled to r x r
    LoaderFontScale,        ///< MENU_LOADER: same r; img.getScaledCopy(r / 48f)
    MenuBgCover,            ///< MENU_BG: alpha 0.9, "cover" fit of the whole screen
};

/** Image file type bits (the Java IMG_PNG / IMG_JPG). */
constexpr unsigned char IMG_PNG = 1, IMG_JPG = 2;

namespace gameimage_detail {
/** One row of the generated table (static data, no constructors run at start-up). */
struct GameImageInfo {
    const char* name;            ///< the Java enum constant name ("CURSOR")
    const char* filename;        ///< base file name ("cursor")
    const char* filenameFormat;  ///< animated-frame pattern ("play-skip-%d"), "" = not animated
    unsigned char type;          ///< IMG_PNG | IMG_JPG
    bool beatmapSkinnable;
    bool preload;
    bool bgLocked;               ///< may only come from the skin that provides SCOREBAR_BG
    bool optional;               ///< may be missing everywhere (getImage() returns null instead of an error)
    bool scalesWithUI;           ///< Java scalesWithUI() override: false keeps the loaded pixel size
    bool allowsSkin;             ///< Java allowsSkin() override: false = only the built-in image is ever used
    ProcKind proc;
    float p1, p2;                ///< ProcKind parameters (the Java literals)
};
extern const GameImageInfo kGameImageTable[];
extern const int kGameImageCount;
}  // namespace gameimage_detail

/** The part of itdelatrisu.opsu.skins.Skin that GameImage uses (the skins/ package is a later task). */
class SkinInfo {
public:
    virtual ~SkinInfo() = default;
    /** Skin.getDirectory(): the game skin folder, or "" for the default skin (Java: null File). */
    virtual std::string getDirectory() const { return ""; }
    /** Skin.getAnimationFramerate(): -1 = one frame per image of the animation. */
    virtual int getAnimationFramerate() const { return -1; }
    /** skin.ini HitCirclePrefix / ScorePrefix / ComboPrefix (defaults of a skin without them). */
    virtual std::string getHitCircleFontPrefix() const { return "default"; }
    virtual std::string getScoreFontPrefix() const { return "score"; }
    virtual std::string getComboFontPrefix() const { return "score"; }
    /** Skin.hasCustomMenuGlow() / getMenuGlowColor(): the built-in logo, play, exit and downloads sprites get their hue rotated. */
    virtual bool hasCustomMenuGlow() const { return false; }
    virtual Color getMenuGlowColor() const { return Color(1.f, 1.f, 1.f, 1.f); }
};

/**
 * Everything GameImage takes from the rest of the game. Set once at start-up with gameimage::setHost(); every member that is
 * left empty keeps the default noted on it.
 */
struct GameImageHost {
    /** Options.getMobileUIScale(def). Default: returns `def`. */
    std::function<float(float def)> getMobileUIScale;
    /** Options.loadHDImages(). Default: false. */
    std::function<bool()> loadHDImages;
    /** Options.isBeatmapSkinIgnored(). Default: false. */
    std::function<bool()> isBeatmapSkinIgnored;
    /**
     * ResourceLoader.resourceExists(name) for the built-in images: `name` is the bare file name ("hitcircle.png",
     * "sliderb0@2x.png"); the file is the APK asset "res/<name>". Not checked with std::filesystem (assets are not files).
     * Default: false (no built-in images).
     */
    std::function<bool(const std::string& name)> resourceExists;
    /** Fonts.LARGE.getLineHeight() / Fonts.DEFAULT.getLineHeight() (MENU_MUSICNOTE, MENU_LOADER). Default: 0. */
    std::function<int()> fontLargeLineHeight, fontDefaultLineHeight;
    /**
     * SkinCache.isListed(dir, name): a cached "is this file in the skin folder" answer. Return 1 / 0 for known present /
     * absent, -1 for unknown (then the file system is asked). Default: always -1.
     */
    std::function<int(const std::string& dir, const std::string& name)> isListed;
};

namespace gameimage {

constexpr float PARALLAX_SCALE = 1.008f;
constexpr const char* HD_SUFFIX = "@2x";

// ---- seams -------------------------------------------------------------------------------------------------------
void setHost(const GameImageHost& host);
/**
 * Options.getSkin(). Default: a SkinInfo with no directory (the default skin, only res/ images). nullptr is the Java
 * "no skin set yet" state: nothing is loaded and getImage() returns null until a skin is set.
 */
void setSkinInfo(std::shared_ptr<const SkinInfo> skin);
const SkinInfo* skinInfo();

// ---- static (Java static methods) ---------------------------------------------------------------------------------
/** Container size; also sets the UI scale (height / 768). Does not reload anything. */
void init(int width, int height);
float getUIscale();
/** Java clearReferences(): forgets every image WITHOUT destroying it (the images are released when no one else holds them). */
void clearReferences();
/** Number of entries (GameImage.values().length). */
int count();
/** All entries, in Java order (GameImage.values()). */
const std::vector<GameImage>& values();
/** Enum constant name (Java name()). */
const char* name(GameImage g);
/** Java valueOf(String); false if there is no such entry. */
bool valueOf(const std::string& name, GameImage& out);
/** Static table row of an entry. */
const gameimage_detail::GameImageInfo& info(GameImage g);

/** Java GameImage.Snapshot: the loaded game skin state of one entry (for SkinCache). Holds shared_ptrs, so copies are cheap. */
struct Snapshot {
    std::shared_ptr<Image> defaultImage;
    std::vector<std::shared_ptr<Image>> defaultImages;
    bool isSkinned = false, absentInSkin = false;
    int rawWidth = 0, rawHeight = 0;
};
using SnapshotMap = std::map<GameImage, Snapshot>;

/**
 * Removes every loaded game skin image from the entries and returns it WITHOUT destroying anything. Beatmap skin images are
 * destroyed first (the stored images may have been processed relative to them). The entries reload lazily afterwards.
 */
SnapshotMap detachLoadedImages();
/** Puts previously detached images back (the entries must not hold any loaded image). */
void attachSnapshots(const SnapshotMap& map);
/** Destroys the images held by the snapshots and empties the map. */
void destroySnapshots(SnapshotMap& map);
/** Loads the default images of every preloaded entry (isPreload()). The Java also prefetch-decodes in the background; not ported (Image::prefetch is a no-op). */
void loadPreloadImages();

/** Java GameImage.scaleMin1px: a uniformly scaled copy that is never smaller than 1x1 pixel. */
Image scaleMin1px(const Image& img, float scale);

// ---- per image (Java instance methods) ----------------------------------------------------------------------------
bool isBeatmapSkinnable(GameImage g);
bool isPreload(GameImage g);

/** The beatmap skin image if there is one, else the default image; null if there is none. Loads the default on first use. */
std::shared_ptr<Image> getImage(GameImage g);
/** Ignores beatmap skin images. */
std::shared_ptr<Image> getDefaultImage(GameImage g);
/** Beatmap skin frames if there are any, else the default frames; empty = Java null. The vector is the live one (setImage(img, i) changes it). */
const std::vector<std::shared_ptr<Image>>& getImages(GameImage g);
const std::vector<std::shared_ptr<Image>>& getDefaultImages(GameImage g);
/** Animation frames if the skin has them, otherwise the single image as one frame; empty if there is no image at all. */
std::vector<std::shared_ptr<Image>> getFrames(GameImage g);
std::shared_ptr<Image> getFirstFrame(GameImage g);
/** The skin's own plain image (hit300) or else the built-in one; never an animation frame. */
std::shared_ptr<Image> getPlainImage(GameImage g);
/** Animation over getImages() (or the single image); frame time 1000 / skin framerate (-1: one frame per second of images, like the Java). */
Animation getAnimation(GameImage g);
Animation getAnimation(GameImage g, int duration);
/** getImage() != null || getImages() != null. */
bool exists(GameImage g);

/** Java setImage(img) / setImage(img, index): replace the (beatmap skin, else default) image. index past the end is ignored. */
void setImage(GameImage g, std::shared_ptr<Image> img);
void setImage(GameImage g, std::shared_ptr<Image> img, int index);

/** Loads the default image(s) if that has not happened (or was not possible) yet. Does nothing while a skin is not set. */
void setDefaultImage(GameImage g);
/**
 * Java setBeatmapSkinImage(File dir): looks in the beatmap skin folder `dir` ("" = none, returns false). Returns true if a new
 * beatmap skin image was loaded. (The brief calls this setSkinImage; the Java name is kept.)
 */
bool setBeatmapSkinImage(GameImage g, const std::string& dir);

/**
 * Java dispose(): destroys the beatmap skin image and the default image (single images only: like the Java, the frame arrays
 * are not touched) and forgets them; resets absentInSkin and the raw size. isSkinned is not reset (Java too).
 */
void dispose(GameImage g);
/** Destroys the beatmap skin image(s), if any. */
void destroyBeatmapSkinImage(GameImage g);

int getRawWidth(GameImage g);
int getRawHeight(GameImage g);
/** The default image comes from the game skin (false: from res/). */
bool hasGameSkinImage(GameImage g);
bool hasBeatmapSkinImage(GameImage g);
bool hasBeatmapSkinImages(GameImage g);
bool isProvidedByBeatmapSkin(GameImage g);
bool isProvidedByGameSkin(GameImage g);

}  // namespace gameimage
}  // namespace opsu
