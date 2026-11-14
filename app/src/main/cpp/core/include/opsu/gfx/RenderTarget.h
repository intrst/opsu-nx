#pragma once
#include <memory>

#include "opsu/gfx/IRenderBackend.h"
#include "opsu/gfx/Texture.h"

namespace opsu {

class Graphics;

/**
 * Offscreen colour target (FBO). Port of itdelatrisu.opsu.render.Rendertarget + the FrameBuffer behind
 * fake.Image(int, int), for SliderBodyRenderer / FrameBufferCache and the like.
 *
 * bind() draws into the target until unbind(); unbind() restores the previous target, its viewport and its clip.
 * Inside, the game's coordinates are the target's pixels (0..width x 0..height, y down) unless setVirtualSize() says
 * otherwise. The Java keeps the screen's camera while an FBO is bound (the whole screen space is squeezed into it);
 * if the ported slider code relies on that, call setVirtualSize(screenW, screenH). Contents are lost with the graphics
 * context (they are redrawn every frame by the callers).
 */
class RenderTarget : public std::enable_shared_from_this<RenderTarget> {
public:
    /** null if there is no render device or the target cannot be created. */
    static std::shared_ptr<RenderTarget> create(int width, int height);
    ~RenderTarget();

    int width() const { return w_; }
    int height() const { return h_; }

    void bind();
    void unbind();
    bool isBound() const { return bound_ > 0; }

    /** The colour attachment; its rowZeroIsBottom() is set so that drawing it gives an upright picture. */
    std::shared_ptr<ITexture> getTexture() const;
    TextureRegion getRegion() const;
    /** Draws the target's picture (Rendertarget.draw). */
    void draw(float x, float y, float w, float h);

    void setVirtualSize(float w, float h);
    /** The Graphics of this target (Image.getGraphics for offscreen images); for Graphics::setCurrent. */
    std::shared_ptr<Graphics> getGraphics();

    void destroy();
    bool isDestroyed() const { return !impl_; }

private:
    RenderTarget(std::shared_ptr<IRenderTarget> impl, int w, int h);

    std::shared_ptr<IRenderTarget> impl_;
    std::shared_ptr<Graphics> graphics_;
    int w_, h_;
    float virtualW_, virtualH_;
    int bound_ = 0;
};

}  // namespace opsu
