#include <android/log.h>
#include <android_native_app_glue.h>

void android_main(android_app* app) {
    __android_log_print(ANDROID_LOG_INFO, "opsu", "native start");
    while (!app->destroyRequested) {
        int events; android_poll_source* src;
        while (ALooper_pollOnce(-1, nullptr, &events, (void**)&src) >= 0)
            if (src) src->process(app, src);
    }
}
