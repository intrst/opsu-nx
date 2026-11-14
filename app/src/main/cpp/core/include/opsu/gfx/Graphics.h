#pragma once
#include <memory>
#include <vector>

#include "opsu/gfx/DrawMode.h"
#include "opsu/gfx/IRenderBackend.h"
#include "opsu/gfx/Transform2D.h"
#include "opsu/gfx/ViewportMap.h"
#include "opsu/util/Color.h"

namespace opsu {

class Font;
class Image;
class RenderTarget;

/**
 * Port of fluddokt.opsu.fake.Graphics: the object the whole game draws through. Pure code: it keeps the state
 * (colour, transform stack, clip, draw mode, line width), applies the transform on the CPU, tessellates shapes and
 * text, and hands triangle lists to an IRenderBackend (GLES now, Vulkan later). Nothing here knows GL.
 *
 * Like the Java, the drawing state is SHARED by all Graphics objects (the screen one and the ones of render
 * targets): they only differ in which target they bind. Drawing through any of them goes to the current target.
 *
 * Differences from the Java (all on purpose, see docs/agents/T9a.md):
 *  - lines/rects/ovals are quads of `lineWidth` SCREEN pixels, not GL_LINES (line widths > 1 do not work on most GLES drivers);
 *  - fillArc also accepts w != h (the Java throws);
 *  - the transform is applied per vertex on the CPU, so a transform change does not flush the batch.
 */
class Graphics {
public:
    static constexpr int MODE_NORMAL = drawmode::NORMAL;
    static constexpr int MODE_ALPHA_MAP = drawmode::ALPHA_MAP;
    static constexpr int MODE_ALPHA_BLEND = drawmode::ALPHA_BLEND;
    static constexpr int MODE_ADD = drawmode::ADD;

    /** Creates the screen Graphics over `backend` (Graphics.init). It becomes getGraphics() and the current one. */
    static std::shared_ptr<Graphics> createScreen(IRenderBackend& backend);
    /** The screen Graphics (Graphics.getGraphics()). Throws std::logic_error before createScreen(). */
    static Graphics& getGraphics();
    static bool hasGraphics();
    /** Graphics.setCurrent: draw into `g`'s target from now on (flushes the batch first). */
    static void setCurrent(Graphics& g);
    static Graphics& current();

    ~Graphics();
    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    // ---- surface -------------------------------------------------------------------------------------------
    /** Graphics.resize: the window surface is now w x h pixels; the virtual size follows it unless set. */
    void resize(int pixelW, int pixelH);
    /** Game coordinate space different from the surface (default: the surface size, as in the Java). */
    void setVirtualResolution(float w, float h, ViewportMap::Mode mode = ViewportMap::Mode::Stretch);
    /** The map of the current target (what the projection and the scissor use). */
    const ViewportMap& viewportMap() const;

    // ---- state ---------------------------------------------------------------------------------------------
    void setBackground(const Color& c);
    const Color& getBackground() const;
    void setColor(const Color& c);
    /** Colour rgb with alpha = c.a * alpha. getColor() still returns c. */
    void setColorAlpha(const Color& c, float alpha);
    const Color& getColor() const;
    void setAntiAlias(bool) {}  // TODO in the Java too
    void setLineWidth(float w);
    void resetLineWidth() { setLineWidth(1.f); }
    float getLineWidth() const;
    void setDrawMode(int mode);  ///< throws std::invalid_argument for an unknown mode (Java: Error)
    int getDrawMode() const;
    /** Pixel snapping (Graphics.setPixelSnap): only has an effect while the transform scales. */
    void setPixelSnap(bool on);
    bool isPixelSnap() const;

    // ---- transform -----------------------------------------------------------------------------------------
    void pushTransform();
    void popTransform();
    void rotate(float x, float y, float angleDegrees);
    void translate(float x, float y);
    void scale(float sx, float sy);
    void resetTransform();
    const Transform2D& transform() const;

    // ---- clip ----------------------------------------------------------------------------------------------
    void setClip(int x, int y, int w, int h);
    void clearClip();

    // ---- images and text -----------------------------------------------------------------------------------
    void drawImage(Image& image, float x, float y);
    /** Draws the region with its centre as the origin (Graphics.drawTexture(TextureRegion...)). A region without a texture is skipped. */
    void drawTexture(const TextureRegion& region, float x, float y, float w, float h, float rotationDegrees);
    void drawTextureEx(const TextureRegion& region, float x, float y, float originX, float originY, float w, float h,
                       float scaleX, float scaleY, float rotationDegrees);
    /** A whole texture, upright (for render targets). */
    void drawTexture(const std::shared_ptr<ITexture>& tex, float x, float y, float w, float h);
    void setFont(std::shared_ptr<Font> font);
    void drawString(const std::string& str, float x, float y);
    void drawString(Font& font, const std::string& str, float x, float y);

    // ---- shapes --------------------------------------------------------------------------------------------
    void fillRect(float x, float y, float w, float h);
    void fillQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);
    void fillGradientRectH(float x, float y, float w, float h, const Color& left, const Color& right);
    void fillGradientRectV(float x, float y, float w, float h, const Color& top, const Color& bottom);
    void drawRect(float x, float y, float w, float h);
    void drawLine(float x1, float y1, float x2, float y2);
    void drawOval(float x, float y, float w, float h);
    void fillArc(float x, float y, float w, float h, float startDegrees, float endDegrees);
    void fillRoundRect(float x, float y, float w, float h, float margin);  ///< plain rect, like the Java (TODO there)
    void drawRoundRect(float x, float y, int w, int h, int margin);        ///< plain rect outline, like the Java
    /** Triangles in game coordinates (transform applied), colour = current colour. For meshes. */
    void fillTriangles(const float* xy, int triangleCount);
    void beginAdditiveShapes();
    void endAdditiveShapes();

    // ---- frame ---------------------------------------------------------------------------------------------
    /** Clears this Graphics' target with the background colour. */
    void clear();
    void clearAlphaMap() {}  // empty in the Java too
    void flush();

    // ---- render targets (used by RenderTarget / Image(w, h)) ------------------------------------------------
    /** Graphics of an offscreen target: shares all state, binds `target` when made current. */
    static std::shared_ptr<Graphics> createForTarget(const std::shared_ptr<IRenderTarget>& target, float virtualW, float virtualH);
    /** Bind `target` on top of the current one and remember target, viewport and clip, for popTarget(). */
    void pushTarget(IRenderTarget* target, const ViewportMap& vm);
    void popTarget();

private:
    struct Shared;
    Graphics(std::shared_ptr<Shared> s, std::shared_ptr<IRenderTarget> target, float vw, float vh);

    void bindThis();
    void emitQuad(ITexture* tex, const float px[8], float u0, float v0, float u1, float v1, const uint8_t rgba[4]);
    void emitSolidQuad(const float px[8], const uint8_t c[4]);
    void emitSolidQuad4(const float px[8], const uint8_t c[16]);
    void emitLine(float x1, float y1, float x2, float y2);
    void emitTri(const float p[6], const uint8_t rgba[4]);
    void submit(ITexture* tex);
    /** Hands the vertices collected so far to the backend (needed before every state change). */
    void sendPending();
    void applyDrawMode();
    void drawGlyphs(Font& font, const std::string& str, float x, float y);

    std::shared_ptr<Shared> s_;
    std::shared_ptr<IRenderTarget> target_;
    float virtualW_, virtualH_;
    TransformStack stack_;
};

}  // namespace opsu
