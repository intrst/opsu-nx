#pragma once
#include <vector>

#include "GlesGl.h"
#include "opsu/gfx/IRenderBackend.h"

namespace opsu {
namespace gles {

class GlesDevice;
class GlesRenderTarget;

/**
 * IRenderBackend over OpenGL ES 3.0. One dynamic vertex buffer; the batch is submitted when the texture, draw mode,
 * scissor, target changes, when the buffer is full, or on flush(). All calls are no-ops while there is no GL context
 * (between GlesWindow::term() and the next init()); the pending batch is dropped then.
 */
class GlesBackend : public IRenderBackend {
public:
    explicit GlesBackend(GlesDevice& dev) : dev_(dev) {}

    // IRenderBackend
    void drawTriangles(ITexture* tex, const Vertex* vertices, int vertexCount) override;
    void setDrawMode(const DrawModeState& mode) override;
    void setScissor(bool enabled, const IRect& rect) override;
    void setTarget(IRenderTarget* target, const ViewportMap& vm) override;
    void clear(const Color& color) override;
    void flush() override { flushBatch(); }

    // context life cycle (called by GlesDevice)
    bool initGl();     ///< a context is current: program, buffers, white texture, fixed state
    void loseGl();     ///< the context is gone: forget the GL names without calling GL
    bool live() const { return live_; }

    /** Start of a frame: clears the WHOLE surface (also letterbox bars) and re-applies the cached state. */
    void beginFrame(const Color& clearColor, int surfaceW, int surfaceH);
    /** Binds a texture on unit 0 (keeps the cache in step with out-of-band uploads). */
    void bindTexture(GLuint id);
    /** A GL texture is about to be deleted: submit a batch that uses it and drop it from the bind cache. */
    void textureDeleted(GLuint id);
    /** Re-binds the current target (after an FBO was created/changed out of band). */
    void rebindTarget();
    void setSurfaceTarget(const ViewportMap& vm) { setTarget(nullptr, vm); }

    int drawCallsLastFrame() const { return lastDrawCalls_; }
    int drawCalls() const { return drawCalls_; }

private:
    void flushBatch();
    void applyMode();
    void applyScissor();
    void applyTarget();

    GlesDevice& dev_;
    bool live_ = false;
    GLuint program_ = 0, vao_ = 0, vbo_ = 0, white_ = 0;
    GLint uProj_ = -1, uTex_ = -1;
    std::vector<Vertex> verts_;
    GLuint batchTex_ = 0;
    GLuint boundTex_ = 0;

    DrawModeState mode_;
    bool scissorOn_ = false;
    IRect scissor_;
    GlesRenderTarget* target_ = nullptr;
    ViewportMap vm_;
    int drawCalls_ = 0, lastDrawCalls_ = 0;
};

}  // namespace gles
}  // namespace opsu
