// Port of itdelatrisu.opsu.options.OptionGroup (the static ALL_OPTIONS / LOADER_OPTIONS lists).
#include "opsu/options/OptionGroup.h"

#include <initializer_list>
#include <memory>
#include <mutex>

namespace opsu {

namespace {

using Icon = OptionGroup::IconKind;

std::mutex g_mu;
std::unique_ptr<std::vector<OptionGroup>> g_all, g_loader;

// Java desktopOnly(): the option on desktop, the NULL placeholder elsewhere (dropped by group()).
GameOption desktopOnly(const GameOption& option, bool desktop) { return desktop ? option : GameOption::NULL_OPTION; }

// Java group(): leaves out NULL placeholders; an empty group is dropped (null in Java, skipped by build()).
void addGroup(std::vector<OptionGroup>& out, const char* category, std::initializer_list<GameOption> options) {
    std::vector<GameOption> list;
    for (const GameOption& o : options)
        if (o != GameOption::NULL_OPTION) list.push_back(o);
    if (!list.empty()) out.emplace_back(category, std::move(list));
}

std::vector<OptionGroup> buildAll(bool desktop) {
    using G = GameOption;
    std::vector<OptionGroup> v;
    // ---- General ----
    v.emplace_back("General", Icon::GENERAL);
    addGroup(v, "LANGUAGE", {G::SHOW_UNICODE});
    addGroup(v, "MISCELLANEOUS", {/* G::ENABLE_WATCH_SERVICE, */ G::SHOW_INTRO, G::LOAD_VERBOSE});
    // ---- Graphics ----
    v.emplace_back("Graphics", Icon::GRAPHICS);
    addGroup(v, "LAYOUT", {desktopOnly(G::SCREEN_RESOLUTION, desktop), /* G::FULLSCREEN, */ G::MOBILE_UI_SCALING});
    addGroup(v, "RENDERER", {desktopOnly(G::TARGET_FPS, desktop), G::SHOW_FPS});
    addGroup(v, "DETAIL SETTINGS",
             {G::SNAKING_SLIDERS, G::SHOW_COMBO_BURSTS, G::SHOW_HIT_LIGHTING, G::SHOW_HIT_STARS, G::SHOW_PERFECT_HIT,
              G::SHOW_FOLLOW_POINTS, G::SONG_THUMBNAILS /* , G::SCREENSHOT_FORMAT */});
    addGroup(v, "SLIDERS", {G::SLIDER_MERGE, G::SLIDER_SHRINK, G::SLIDER_CAPS});
    addGroup(v, "MAIN MENU", {G::DYNAMIC_BACKGROUND, G::PARALLAX, G::ENABLE_THEME_SONG});
    // ---- Gameplay ----
    v.emplace_back("Gameplay", Icon::GAMEPLAY);
    addGroup(v, "GENERAL",
             {G::BACKGROUND_DIM, G::BACKGROUND_BLUR, G::ENABLE_VIDEOS, G::ENABLE_STORYBOARD, G::PLAYFIELDSCALE,
              G::FORCE_DEFAULT_PLAYFIELD, G::IN_GAME_PAUSE, G::PLAYER_LOADER});
    addGroup(v, "ON-SCREEN DISPLAY", {G::SHOW_HIT_ERROR_BAR, G::AVGOFFSETDISP, G::ALWAYS_SHOW_KEY_OVERLAY, G::SCOREBOARD});
    addGroup(v, "SPINNERS", {G::DISABLE_SPINNER_UI, G::DISABLE_SPINNER_SOUND});
    // ---- Audio ----
    v.emplace_back("Audio", Icon::AUDIO);
    addGroup(v, "VOLUME", {G::MASTER_VOLUME, G::MUSIC_VOLUME, G::EFFECT_VOLUME, G::HITSOUND_VOLUME});
    addGroup(v, "SOUND EFFECTS", {G::DISABLE_GAMEPLAY_SOUNDS, G::DISABLE_SOUNDS});
    addGroup(v, "OFFSET ADJUSTMENT", {G::MUSIC_OFFSET /* , G::RENDER_OFFSET */});
    // ---- Skin ----
    v.emplace_back("Skin", Icon::SKIN);
    addGroup(v, "SKIN", {G::SKIN, G::LOAD_HD_IMAGES, G::IGNORE_BEATMAP_SKINS});
    addGroup(v, "CURSOR", {G::FORCE_SKIN_CURSOR, G::CURSOR_SIZE, G::DISABLE_CURSOR});
    // ---- Input ----
    v.emplace_back("Input", Icon::INPUT);
    addGroup(v, "TOUCH", {G::MULTITOUCH_GAMEPLAY, G::TOUCH_MAX_FINGERS, G::TOUCH_PALM_REJECTION});
    addGroup(v, "MOUSE", {G::DISABLE_MOUSE_WHEEL, G::DISABLE_MOUSE_BUTTONS});
    addGroup(v, "KEYBOARD", {G::KEY_LEFT, G::KEY_RIGHT});
    // ---- Custom ----
    v.emplace_back("Custom", Icon::CUSTOM);
    addGroup(v, "DIFFICULTY", {G::FIXED_CS, G::FIXED_HP, G::FIXED_AR, G::FIXED_OD, G::FIXED_SPEED});
    addGroup(v, "SEEKING", {G::CHECKPOINT, G::REPLAY_SEEKING});
    return v;
}

std::vector<OptionGroup> buildLoader() {
    using G = GameOption;
    std::vector<OptionGroup> v;
    v.emplace_back("Gameplay", Icon::GAMEPLAY);
    addGroup(v, "VISUAL SETTINGS",
             {G::BACKGROUND_DIM, G::BACKGROUND_BLUR, G::ENABLE_VIDEOS, G::ENABLE_STORYBOARD, G::IGNORE_BEATMAP_SKINS});
    v.emplace_back("Audio", Icon::AUDIO);
    addGroup(v, "AUDIO SETTINGS", {G::MUSIC_VOLUME, G::HITSOUND_VOLUME, G::DISABLE_GAMEPLAY_SOUNDS, G::MUSIC_OFFSET});
    v.emplace_back("Input", Icon::INPUT);
    addGroup(v, "INPUT SETTINGS", {G::DISABLE_MOUSE_BUTTONS});
    return v;
}

bool desktopSeam() {
    // read through the public seam holder: Options keeps it private, so ask via a tiny probe
    return Options::isDesktopPlatform();
}

}  // namespace

std::vector<OptionGroup>& OptionGroup::allOptions() {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_all) g_all = std::make_unique<std::vector<OptionGroup>>(buildAll(desktopSeam()));
    return *g_all;
}

std::vector<OptionGroup>& OptionGroup::loaderOptions() {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_loader) g_loader = std::make_unique<std::vector<OptionGroup>>(buildLoader());
    return *g_loader;
}

void OptionGroup::rebuild() {
    std::lock_guard<std::mutex> lk(g_mu);
    g_all.reset();
    g_loader.reset();
}

}  // namespace opsu
