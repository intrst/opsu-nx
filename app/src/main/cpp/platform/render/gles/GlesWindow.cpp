#include "GlesWindow.h"

#include <cmath>

#include "../AssetFiles.h"
#include "GlesDevice.h"
#include "GlesGl.h"
#include "opsu/gfx/Colors.h"
#include "opsu/gfx/Image.h"

#define LOG_TAG "opsu-gles"

namespace opsu {
namespace gles {

namespace {
constexpr EGLint kEs3Bit = 0x0040;  // EGL_OPENGL_ES3_BIT (EGL_OPENGL_ES3_BIT_KHR)
}

struct GlesWindow::Scene {
    std::unique_ptr<Image> cursor;
    float t = 0.f;
    float fpsTime = 0.f;
    int frames = 0;
};

GlesWindow::GlesWindow() = default;

GlesWindow::~GlesWindow() {
    term();
    scene_.reset();
    graphics_.reset();
    if (device_) setRenderDevice(nullptr);
    device_.reset();
    if (display_ != EGL_NO_DISPLAY) eglTerminate(display_);
}

void GlesWindow::setAssetManager(AAssetManager* mgr) { render::setAssetManager(mgr); }
void GlesWindow::setDebugChecks(bool on) { g_glDebug = on; }

int GlesWindow::drawCallsLastFrame() const { return device_ ? device_->gl().drawCallsLastFrame() : 0; }

bool GlesWindow::createSurfaceAndContext() {
    if (display_ == EGL_NO_DISPLAY) {
        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major = 0, minor = 0;
        if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, &major, &minor)) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "eglInitialize failed (0x%x)", static_cast<unsigned>(eglGetError()));
            display_ = EGL_NO_DISPLAY;
            return false;
        }
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "EGL %d.%d", major, minor);
    }
    const EGLint cfgAttribs[] = {EGL_RENDERABLE_TYPE, kEs3Bit, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                                 EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_NONE};
    EGLint numConfigs = 0;
    if (!eglChooseConfig(display_, cfgAttribs, &config_, 1, &numConfigs) || numConfigs < 1) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "no ES 3.0 RGBA8888 config (0x%x)", static_cast<unsigned>(eglGetError()));
        return false;
    }
    EGLint format = 0;
    eglGetConfigAttrib(display_, config_, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window_, 0, 0, format);

    surface_ = eglCreateWindowSurface(display_, config_, window_, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "eglCreateWindowSurface failed (0x%x)", static_cast<unsigned>(eglGetError()));
        return false;
    }
    const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, ctxAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "eglCreateContext(ES 3.0) failed (0x%x)", static_cast<unsigned>(eglGetError()));
        destroySurfaceAndContext();
        return false;
    }
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "eglMakeCurrent failed (0x%x)", static_cast<unsigned>(eglGetError()));
        destroySurfaceAndContext();
        return false;
    }
    eglSwapInterval(display_, 1);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "GL_RENDERER=%s GL_VERSION=%s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                        reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    return true;
}

void GlesWindow::destroySurfaceAndContext() {
    if (display_ == EGL_NO_DISPLAY) return;
    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
    if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
    context_ = EGL_NO_CONTEXT;
    surface_ = EGL_NO_SURFACE;
}

void GlesWindow::queryAndApplySize() {
    EGLint w = 0, h = 0;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &w);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &h);
    if (w < 1 || h < 1) return;
    const bool changed = w != width_ || h != height_;
    width_ = w;
    height_ = h;
    device_->setSurfaceSize(w, h);
    if (graphics_) graphics_->resize(w, h);
    if (changed) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "surface %dx%d", w, h);
}

bool GlesWindow::init(ANativeWindow* window) {
    if (!window) return false;
    if (ready_ || context_ != EGL_NO_CONTEXT) term();
    window_ = window;
    if (!device_) {
        device_ = std::make_shared<GlesDevice>();
        setRenderDevice(device_);
    }
    if (!createSurfaceAndContext()) { window_ = nullptr; return false; }
    if (!device_->onContextCreated()) {
        destroySurfaceAndContext();
        window_ = nullptr;
        return false;
    }
    if (!graphics_) graphics_ = Graphics::createScreen(device_->backend());
    queryAndApplySize();
    graphics_->setDrawMode(graphics_->getDrawMode());  // the new context starts in the default state
    ready_ = true;
    return true;
}

void GlesWindow::onResize() {
    if (!ready_) return;
    queryAndApplySize();
}

void GlesWindow::term() {
    if (graphics_ && ready_) graphics_->flush();
    if (device_) device_->onContextLost();
    destroySurfaceAndContext();
    ready_ = false;
    window_ = nullptr;
}

bool GlesWindow::beginFrame() {
    if (!ready_) return false;
    device_->gl().beginFrame(graphics_->getBackground(), width_, height_);
    return true;
}

void GlesWindow::endFrame() {
    if (!ready_) return;
    graphics_->flush();
    if (eglSwapBuffers(display_, surface_)) return;
    const EGLint err = eglGetError();
    if (err == EGL_CONTEXT_LOST) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "EGL_CONTEXT_LOST: recreating the context");
        ANativeWindow* w = window_;
        term();
        if (w) init(w);
    } else {
        // EGL_BAD_SURFACE / EGL_BAD_NATIVE_WINDOW: the window is going away; APP_CMD_TERM_WINDOW follows, stop drawing until init()
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "eglSwapBuffers failed (0x%x), waiting for the next window", static_cast<unsigned>(err));
        ready_ = false;
    }
}

void GlesWindow::debugDrawTestScene(AAssetManager* assets, float dt) {
    if (assets) render::setAssetManager(assets);
    if (!ready_ || !graphics_) return;
    if (!scene_) scene_ = std::make_unique<Scene>();
    Scene& s = *scene_;
    if (!s.cursor) s.cursor = std::make_unique<Image>("res/cursor.png");
    Graphics& g = *graphics_;
    const float W = static_cast<float>(width_), H = static_cast<float>(height_);
    s.t += dt;

    g.resetTransform();
    g.setDrawMode(Graphics::MODE_NORMAL);
    g.fillGradientRectV(0.f, 0.f, W, H, Color(0.10f, 0.10f, 0.20f, 1.f), Color(0.02f, 0.02f, 0.05f, 1.f));

    const float cw = static_cast<float>(s.cursor->getWidth()), ch = static_cast<float>(s.cursor->getHeight());

    // 1. a sprite at an angle, scaled 3x, tinted orange, pulsing alpha
    s.cursor->setRotation(s.t * 45.f);
    s.cursor->setAlpha(0.65f + 0.35f * std::sin(s.t * 2.f));
    s.cursor->draw(W * 0.25f - cw * 1.5f, H * 0.30f - ch * 1.5f, cw * 3.f, ch * 3.f, Color(1.f, 0.6f, 0.2f, 1.f));
    s.cursor->setAlpha(1.f);
    s.cursor->setRotation(0.f);

    // 2. a rotated + translated group (transform push/pop)
    g.pushTransform();
    g.translate(W * 0.70f, H * 0.30f);
    g.rotate(0.f, 0.f, s.t * 30.f);
    g.setColor(Color(0.2f, 0.4f, 1.f, 1.f));
    g.fillRect(-90.f, -30.f, 180.f, 60.f);
    s.cursor->draw(-cw / 2.f, -ch / 2.f);
    g.drawRect(-90.f, -30.f, 180.f, 60.f);
    g.popTransform();

    // 3. a clipped rectangle: the wide bar and the sprite are cut at the clip box
    const int cx = static_cast<int>(W * 0.08f), cy = static_cast<int>(H * 0.62f), cwid = static_cast<int>(W * 0.30f), chei = static_cast<int>(H * 0.20f);
    g.setClip(cx, cy, cwid, chei);
    g.setColor(Color(0.15f, 0.55f, 0.25f, 1.f));
    g.fillRect(static_cast<float>(cx) - 80.f, static_cast<float>(cy) + chei * 0.3f, static_cast<float>(cwid) + 160.f, chei * 0.4f);
    s.cursor->draw(static_cast<float>(cx) + cwid * (0.5f + 0.6f * std::sin(s.t)) - cw / 2.f, static_cast<float>(cy) + chei / 2.f - ch / 2.f);
    g.clearClip();
    g.setColor(colors::white);
    g.drawRect(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cwid), static_cast<float>(chei));

    // 4. additive blending: the overlap gets brighter
    g.setDrawMode(Graphics::MODE_ADD);
    s.cursor->draw(W * 0.62f, H * 0.62f, cw * 4.f, ch * 4.f, Color(0.9f, 0.4f, 0.1f, 1.f));
    s.cursor->draw(W * 0.62f + cw * 2.f, H * 0.62f + ch * 1.f, cw * 4.f, ch * 4.f, Color(0.1f, 0.4f, 0.9f, 1.f));
    g.setDrawMode(Graphics::MODE_NORMAL);
    g.setColor(colors::white);

    // fps to logcat once a second (an on-screen counter needs fonts: milestone 3)
    s.frames++;
    s.fpsTime += dt;
    if (s.fpsTime >= 1.f) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "fps=%.1f drawCalls=%d", s.frames / s.fpsTime, drawCallsLastFrame());
        s.fpsTime = 0.f;
        s.frames = 0;
    }
}

}  // namespace gles
}  // namespace opsu
