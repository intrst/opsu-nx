#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <chrono>
#include <memory>

#include "opsu/input/TouchRouter.h"
#include "platform/audio/BassBackend.h"
#include "platform/input/AndroidInput.h"
#include "platform/log/AndroidLogger.h"
#include "platform/render/gles/GlesWindow.h"

// Device test entry point. What you should see / hear / get in logcat:
//   adb logcat -s opsu opsu-gles opsu-render BassAudio BassMusic BassDebug
//   - picture: the T9a test scene (sprites, clip box, additive blend, fps in logcat)
//   - sound:   assets/res/theme.mp3 looping
//   - touch:   press / release / click / drag lines (a second finger = right button)
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

void updateInputSize(android_app* app) {
    if (g_input && app->window)
        g_input->setSurfaceSize(ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));
}

void onAppCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                g_window.init(app->window);
                updateInputSize(app);
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            g_window.onResize();
            updateInputSize(app);
            break;
        case APP_CMD_TERM_WINDOW:
            if (g_input) g_input->cancelAll();  // no stuck fingers
            g_window.term();
            break;
        case APP_CMD_LOST_FOCUS:
        case APP_CMD_PAUSE:
            if (g_input) g_input->cancelAll();
            break;
    }
}

}  // namespace

void android_main(android_app* app) {
    opsu::installAndroidLogger("", false);  // logcat only for now (no log file)
    __android_log_print(ANDROID_LOG_INFO, "opsu", "native start, version %s", OPSU_VERSION);

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

        if (g_window.hasSurface() && g_window.beginFrame()) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            g_window.debugDrawTestScene(app->activity->assetManager, dt);
            g_window.endFrame();
        }
    }

    g_input = nullptr;
    g_window.term();
    if (music) music->dispose();
    music.reset();
    BassAudio::free();
}
