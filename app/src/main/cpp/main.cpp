#include <android/log.h>
#include <android_native_app_glue.h>

#include <memory>

#include "platform/audio/BassBackend.h"
#include "platform/log/AndroidLogger.h"

// First-device-test entry point: logs to logcat and loops assets/res/theme.mp3 through BASS.
// Check with: adb logcat -s opsu BassAudio BassMusic BassDebug
void android_main(android_app* app) {
    opsu::installAndroidLogger("", false);  // logcat only for now (no log file)
    __android_log_print(ANDROID_LOG_INFO, "opsu", "native start, version %s", OPSU_VERSION);

    using opsu::bass::BassAudio;
    BassAudio::setJavaVM(app->activity->vm);  // must come before BASS init (hasCode=false build)
    // BassAudio::setPollOutputLatency(true); // off on purpose, see docs/agents/T2.md before enabling

    std::unique_ptr<opsu::AbsMusic> music = opsu::bass::debugPlayAsset(app->activity->assetManager);

    while (!app->destroyRequested) {
        int events;
        android_poll_source* src;
        // wake up about every 16 ms so BASS events are delivered once per "frame"
        while (ALooper_pollOnce(16, nullptr, &events, reinterpret_cast<void**>(&src)) >= 0)
            if (src) src->process(app, src);
        BassAudio::pumpEvents();
    }

    if (music) music->dispose();
    music.reset();
    BassAudio::free();
}
