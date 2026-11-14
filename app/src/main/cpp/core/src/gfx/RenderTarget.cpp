#include "opsu/gfx/RenderTarget.h"

#include "opsu/gfx/Graphics.h"

namespace opsu {

RenderTarget::RenderTarget(std::shared_ptr<IRenderTarget> impl, int w, int h)
    : impl_(std::move(impl)), w_(w), h_(h), virtualW_(static_cast<float>(w)), virtualH_(static_cast<float>(h)) {}

RenderTarget::~RenderTarget() { destroy(); }

std::shared_ptr<RenderTarget> RenderTarget::create(int width, int height) {
    IRenderDevice* dev = renderDevice();
    if (!dev || width < 1 || height < 1) return nullptr;
    std::shared_ptr<IRenderTarget> impl = dev->createRenderTarget(width, height);
    if (!impl) return nullptr;
    return std::shared_ptr<RenderTarget>(new RenderTarget(std::move(impl), width, height));
}

void RenderTarget::bind() {
    if (!impl_) return;
    Graphics::getGraphics().pushTarget(impl_.get(), ViewportMap(virtualW_, virtualH_, w_, h_));
    bound_++;
}

void RenderTarget::unbind() {
    if (bound_ <= 0) return;
    bound_--;
    Graphics::getGraphics().popTarget();
}

std::shared_ptr<ITexture> RenderTarget::getTexture() const { return impl_ ? impl_->texture() : nullptr; }

TextureRegion RenderTarget::getRegion() const {
    TextureRegion r;
    r.tex = getTexture();
    r.w = w_;
    r.h = h_;
    return r;
}

void RenderTarget::draw(float x, float y, float w, float h) {
    if (impl_) Graphics::getGraphics().drawTexture(impl_->texture(), x, y, w, h);
}

void RenderTarget::setVirtualSize(float w, float h) {
    virtualW_ = w;
    virtualH_ = h;
    graphics_.reset();  // made again with the new size on the next getGraphics()
}

std::shared_ptr<Graphics> RenderTarget::getGraphics() {
    if (!impl_) return nullptr;
    if (!graphics_) graphics_ = Graphics::createForTarget(impl_, virtualW_, virtualH_);
    return graphics_;
}

void RenderTarget::destroy() {
    while (bound_ > 0) unbind();
    graphics_.reset();
    impl_.reset();
}

}  // namespace opsu
