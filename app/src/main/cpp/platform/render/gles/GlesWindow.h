#pragma once
#include <EGL/egl.h>
#include <android/asset_manager.h>
#include <android/native_window.h>

#include <memory>

#include "opsu/gfx/Graphics.h"

namespace opsu {

class Image;

namespace gles {

class GlesDevice;

/**
 * EGL window + GLES 3.0 renderer. One instance for the app, used on the render (game) thread only.
 *
 * Life cycle (android_native_app_glue):
 *   APP_CMD_INIT_WINDOW     -> init(app->window)        (also after a lost surface: a new EGL context is made, textures,
 *                                                       glyph pages and atlases are refilled, FBOs come back empty)
 *   APP_CMD_WINDOW_RESIZED
 *   APP_CMD_CONFIG_CHANGED  -> onResize()
 *   APP_CMD_TERM_WINDOW     -> term()                   (releases surface + context, keeps every recreatable resource)
 *   every frame while hasSurface(): beginFrame(); ...draw through graphics()...; endFrame();
 */
class GlesWindow {
public:
    GlesWindow();
    ~GlesWindow();
    GlesWindow(const GlesWindow&) = delete;
    GlesWindow& operator=(const GlesWindow&) = delete;

    /** Needed to read assets/res/... : call once before init() (android_app->activity->assetManager). */
    void setAssetManager(AAssetManager* mgr);
    /** glGetError after the important calls, for debugging only (off by default, it stalls the pipeline). */
    static void setDebugChecks(bool on);

    /** Creates the EGL display/surface/context and the GL state. false on failure (see logcat, tag opsu-gles). */
    bool init(ANativeWindow* window);
    /** After the surface size changed: re-reads the size, updates viewport and projection. */
    void onResize();
    /** Clears (to Graphics' background colour, default black) and starts a frame. Returns false if there is nothing to draw to. */
    bool beginFrame();
    /** Submits the batch and swaps. Detects a lost context/surface and recovers or waits for the next init(). */
    void endFrame();
    /** Releases the surface and the context. */
    void term();

    bool hasSurface() const { return ready_; }
    int width() const { return width_; }
    int height() const { return height_; }
    /** The Graphics the game draws through (valid after the first successful init(), stays the same object afterwards). */
    std::shared_ptr<Graphics> graphics() const { return graphics_; }
    /** Draw calls submitted in the last finished frame (batching check). */
    int drawCallsLastFrame() const;

    /** Draws the device test scene (see docs/agents/T9a.md "device checklist"). */
    void debugDrawTestScene(AAssetManager* assets, float dtSeconds);

private:
    bool createSurfaceAndContext();
    void destroySurfaceAndContext();
    void queryAndApplySize();

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLConfig config_ = nullptr;
    ANativeWindow* window_ = nullptr;
    bool ready_ = false;
    int width_ = 0, height_ = 0;

    std::shared_ptr<GlesDevice> device_;
    std::shared_ptr<Graphics> graphics_;

    struct Scene;
    std::unique_ptr<Scene> scene_;
};

}  // namespace gles
}  // namespace opsu
