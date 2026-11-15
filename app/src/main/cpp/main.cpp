#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <chrono>
#include <memory>
#include <string>

#include "opsu/GameImage.h"
#include "opsu/input/TouchRouter.h"
#include "opsu/options/OptionsWiring.h"
#include "platform/audio/BassBackend.h"
#include "platform/input/AndroidInput.h"
#include "platform/log/AndroidLogger.h"
#include "platform/render/gles/GlesWindow.h"

// Device test entry point. What you should see / hear / get in logcat:
//   adb logcat -s opsu opsu-gles opsu-render BassAudio BassMusic BassDebug
//   - picture: the T9a test scene, plus the game's own cursor image loaded through GameImage (top left)
//   - sound:   assets/res/theme.mp3 looping
//   - touch:   press / release / click / drag lines (a second finger = right button)
//   - logcat:  "options: ..." lines (opsu.cfg read, values, paths) and "gameimage: ..." lines
// Replace the pieces below with the real game one by one.

namespace {

constexpr bool kPlayTestMusic = true;

// Logs every menu-side event. Replace with the real game state later.
struct LogTarget : opsu::InputTarget {
    void mousePressed(int b, int x, int y) override { __android_log_print(ANDROID_LOG_INFO, "opsu", "press b=%d x=%d y=%d", b, x, y); }
    void mouseReleased(int b, int x, int y) override { __android_log_print(ANDROID_LOG_INFO, "opsu", "release b=%d x=%d y=%d", b, x, y); }
    void mouseClicked(int b, int x, int y, int n) override { __android_log_print(ANDROID_LOG_INFO, "opsu", "click b=%d x=%d y=%d n=%d", b, x, y, n); }
    void mouseDragged(int ox, int oy, int nx, int ny) override { __android_log_print(ANDROID_LOG_INFO, "opsu", "drag %d,%d -> %d,%d", ox, oy, nx, ny); }
    void mouseWheelMoved(int a) override { __android_log_print(ANDROID_LOG_INFO, "opsu", "wheel %d", a); }
    void keyPressed(int k, char) override { __android_log_print(ANDROID_LOG_INFO, "opsu", "key down %d", k); }
    void keyReleased(int k, char) override { __android_log_print(ANDROID_LOG_INFO, "opsu", "key up %d", k); }
} g_logTarget;

opsu::gles::GlesWindow g_window;
opsu::AndroidInput* g_input = nullptr;
bool g_loggedGameImage = false;
int g_giW = 0, g_giH = 0;

void updateInputSize(android_app* app) {
    if (g_input && app->window)
        g_input->setSurfaceSize(ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));
}

// GameImage needs the real screen size: a brand-new window can report a 1x1 surface for a moment, and images loaded at that size
// would stay 1x1. So only (re)initialise for a sane size, and drop the cached images so they are reloaded at the new scale.
void refreshGameImage() {
    const int w = g_window.width(), h = g_window.height();
    if (w < 64 || h < 64 || (w == g_giW && h == g_giH)) return;
    g_giW = w;
    g_giH = h;
    opsu::gameimage::init(w, h);
    opsu::gameimage::clearReferences();
    g_loggedGameImage = false;  // log the image size again
}

// Options (T10): init -> seams -> parse. Storage: until the "All files access" permission is requested by a Java activity
// (a later task), the app can only write to its own external files dir, so everything lives there for now:
//   /storage/emulated/0/Android/data/<package>/files/
void setUpOptions(android_app* app) {
    std::string base = app->activity->externalDataPath ? app->activity->externalDataPath
                       : app->activity->internalDataPath ? app->activity->internalDataPath : "/data/local/tmp";
    opsu::OptionsPaths paths;
    paths.configDir = paths.dataDir = paths.cacheDir = base;
    paths.localDir = app->activity->internalDataPath ? app->activity->internalDataPath : base;
    paths.downloadDir = base + "/Download";
    opsu::Options::init(paths);  // no I/O

    opsu::OptionsSeams seams;
    seams.isTouchPlatform = [] { return true; };  // enables isMultitouchGameplay()
    seams.screenWidthInches = [app] {             // UI scale / playfield scale defaults (<= 6.0 in = phone)
        int dpi = AConfiguration_getDensity(app->config);  // density bucket; JNI DisplayMetrics.xdpi would be more exact
        return (dpi > 0 && app->window) ? float(ANativeWindow_getWidth(app->window)) / float(dpi) : 10.0f;
    };
    opsu::OptionsWiring::wireAudioSeams(seams);  // volume changes reach MusicController
    opsu::Options::setSeams(seams);              // before parseOptions: the screen defaults are resolved here
    opsu::Options::parseOptions();               // reads opsu.cfg (writes the defaults if there is none)

    opsu::MusicController::setHost(opsu::OptionsWiring::musicControllerHost());
    opsu::SoundController::setHost(opsu::OptionsWiring::soundControllerHost());
    __android_log_print(ANDROID_LOG_INFO, "opsu", "options: base dir %s, master volume %.2f, music volume %.2f", base.c_str(),
                        opsu::Options::getMasterVolume(), opsu::Options::getMusicVolume());
}

// GameImage (T11): host seams. The skin is the default skin (no folder), so every image comes from assets/res/.
void setUpGameImage(android_app* app) {
    AAssetManager* mgr = app->activity->assetManager;
    opsu::GameImageHost host;
    host.getMobileUIScale = [](float def) { return opsu::Options::getMobileUIScale(def); };
    host.loadHDImages = [] { return opsu::Options::loadHDImages(); };
    host.isBeatmapSkinIgnored = [] { return opsu::Options::isBeatmapSkinIgnored(); };
    host.resourceExists = [mgr](const std::string& name) {  // built-in images are APK assets "res/<name>"
        AAsset* a = AAssetManager_open(mgr, ("res/" + name).c_str(), AASSET_MODE_UNKNOWN);
        if (!a) return false;
        AAsset_close(a);
        return true;
    };
    opsu::gameimage::setHost(host);
}

void onAppCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                g_window.init(app->window);
                updateInputSize(app);
                refreshGameImage();  // before the first getImage()
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            g_window.onResize();
            updateInputSize(app);
            refreshGameImage();
            break;
        case APP_CMD_TERM_WINDOW:
            if (g_input) g_input->cancelAll();  // no stuck fingers
            g_window.term();
            break;
        case APP_CMD_LOST_FOCUS:
        case APP_CMD_PAUSE:
            if (g_input) g_input->cancelAll();
            opsu::Options::flushNow();
            break;
    }
}

}  // namespace

void android_main(android_app* app) {
    opsu::installAndroidLogger("", false);  // logcat only for now (no log file)
    __android_log_print(ANDROID_LOG_INFO, "opsu", "native start, version %s", OPSU_VERSION);

    // ---- settings (T10) and image registry (T11) ----
    setUpOptions(app);
    setUpGameImage(app);

    // ---- audio (T1/T2) ----
    using opsu::bass::BassAudio;
    BassAudio::setJavaVM(app->activity->vm);  // must come before BASS init (hasCode=false build)
    // BassAudio::setPollOutputLatency(true); // off on purpose, see docs/agents/T2.md before enabling
    std::unique_ptr<opsu::AbsMusic> music;
    if (kPlayTestMusic) music = opsu::bass::debugPlayAsset(app->activity->assetManager);

    // ---- input (T4) ----
    opsu::TouchRouter router([] { return &g_logTarget; });  // later: return the current game state
    opsu::AndroidInput input(router);
    g_input = &input;
    app->userData = &input;
    app->onInputEvent = &opsu::AndroidInput::onAppInputEvent;
    // ... and when the game switches screen:  router.onStateEntered();

    // ---- renderer (T9a) ----
    g_window.setAssetManager(app->activity->assetManager);
    app->onAppCmd = onAppCmd;
    auto last = std::chrono::steady_clock::now();

    while (!app->destroyRequested) {
        int events;
        android_poll_source* src;
        // poll without waiting while there is a surface (eglSwapBuffers paces the frames)
        while (ALooper_pollOnce(g_window.hasSurface() ? 0 : 100, nullptr, &events, reinterpret_cast<void**>(&src)) >= 0)
            if (src) src->process(app, src);

        BassAudio::pumpEvents();
        opsu::Options::flushIfDue();

        if (g_window.hasSurface() && g_window.beginFrame()) {
            refreshGameImage();  // cheap: does nothing unless the surface size changed
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            g_window.debugDrawTestScene(app->activity->assetManager, dt);

            // The game's own cursor, loaded through GameImage (res/cursor.png, scaled by the UI scale).
            if (auto cursor = opsu::gameimage::getImage(opsu::GameImage::CURSOR)) {
                g_window.graphics()->drawImage(*cursor, 160.f, 120.f);
                if (!g_loggedGameImage) {
                    g_loggedGameImage = true;
                    __android_log_print(ANDROID_LOG_INFO, "opsu", "gameimage: CURSOR %dx%d, UI scale %.2f", cursor->getWidth(),
                                        cursor->getHeight(), opsu::gameimage::getUIscale());
                }
            } else if (!g_loggedGameImage) {
                g_loggedGameImage = true;
                __android_log_print(ANDROID_LOG_WARN, "opsu", "gameimage: CURSOR not found");
            }

            g_window.endFrame();
        }
    }

    opsu::Options::flushNow();
    g_input = nullptr;
    g_window.term();
    if (music) music->dispose();
    music.reset();
    BassAudio::free();
}
