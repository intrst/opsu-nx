#pragma once
// Single place that pulls in the GL ES 3.0 headers (standard NDK headers, no loader) and the debug error check.
#include <GLES3/gl3.h>
#include <android/log.h>

namespace opsu {
namespace gles {

/** glGetError checks cost a pipeline sync: only on when a debug flag asks for them (GlesWindow::setDebugChecks). */
extern bool g_glDebug;
inline void glCheck(const char* what) {
    if (!g_glDebug) return;
    for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
        __android_log_print(ANDROID_LOG_ERROR, "opsu-gles", "GL error 0x%x after %s", static_cast<unsigned>(e), what);
}

}  // namespace gles
}  // namespace opsu
