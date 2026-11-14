#pragma once
#include <memory>
#include <vector>

#include "../AssetFiles.h"
#include "GlesBackend.h"
#include "opsu/gfx/IRenderBackend.h"

namespace opsu {
namespace gles {

class GlesDevice;

/**
 * A GL texture that survives context loss: the object stays valid, the GL name is re-created (and refilled through the
 * restorer the owner gave) when a new context is made. `gen_` says which context the name belongs to, so a destructor
 * running after a context change never deletes a name that now belongs to something else.
 */
class GlesTexture : public ITexture {
public:
    GlesTexture(GlesDevice& dev, int w, int h, bool rowZeroIsBottom, TextureFilter filter);
    ~GlesTexture() override;

    int width() const override { return w_; }
    int height() const override { return h_; }
    void setFilter(TextureFilter minFilter, TextureFilter magFilter) override;
    void update(int x, int y, int w, int h, const uint8_t* rgba) override;
    bool rowZeroIsBottom() const override { return bottom_; }

    /** GL name valid in the current context, 0 if lost. */
    GLuint glName() const;
    /** Creates the GL texture in the current context; `rgba` may be null. */
    bool createGl(const uint8_t* rgba);
    void restore();
    void setRestorer(IRenderDevice::TextureRestorer r) { restore_ = std::move(r); }
    /** Keep these pixels for the next context (only used when created while there was no context). */
    void setPending(const uint8_t* rgba);

private:
    void applyFilter();

    GlesDevice& dev_;
    int w_, h_;
    bool bottom_;
    TextureFilter minF_, magF_;
    GLuint id_ = 0;
    unsigned gen_ = 0;
    IRenderDevice::TextureRestorer restore_;
    std::vector<uint8_t> pending_;
};

/** FBO with a colour texture. Contents are lost with the context; the FBO is rebuilt empty. */
class GlesRenderTarget : public IRenderTarget {
public:
    GlesRenderTarget(GlesDevice& dev, int w, int h);
    ~GlesRenderTarget() override;
    int width() const override { return w_; }
    int height() const override { return h_; }
    std::shared_ptr<ITexture> texture() override { return tex_; }

    GLuint fboName() const;
    bool createGl();

private:
    GlesDevice& dev_;
    int w_, h_;
    std::shared_ptr<GlesTexture> tex_;
    GLuint fbo_ = 0;
    unsigned gen_ = 0;
};

/** IRenderDevice for GLES 3.0. Owned by GlesWindow, installed with setRenderDevice(). */
class GlesDevice : public IRenderDevice {
public:
    GlesDevice();

    IRenderBackend& backend() override { return backend_; }
    GlesBackend& gl() { return backend_; }
    std::shared_ptr<ITexture> createTexture(int w, int h, const uint8_t* rgba, TextureFilter filter, TextureRestorer restore) override;
    std::shared_ptr<IRenderTarget> createRenderTarget(int w, int h) override;
    IImageDecoder& imageDecoder() override { return decoder_; }
    std::shared_ptr<IFontFace> loadFontFace(const std::string& name, float pixelSize) override;
    int maxTextureSize() const override { return maxTex_; }
    void screenSize(int& w, int& h) const override { w = surfaceW_; h = surfaceH_; }

    void setSurfaceSize(int w, int h) { surfaceW_ = w; surfaceH_ = h; }
    /** True between onContextCreated() and onContextLost(). */
    bool glValid() const { return valid_; }
    unsigned generation() const { return gen_; }

    /** A context is current: build the backend, then re-create and refill every live texture and target. */
    bool onContextCreated();
    /** The context is (about to be) gone. No GL calls are made. */
    void onContextLost();

private:
    template <class T>
    static void prune(std::vector<std::weak_ptr<T>>& v);

    GlesBackend backend_;
    render::StbImageDecoder decoder_;
    std::vector<std::weak_ptr<GlesTexture>> textures_;
    std::vector<std::weak_ptr<GlesRenderTarget>> targets_;
    bool valid_ = false;
    unsigned gen_ = 1;
    int maxTex_ = 2048;
    int surfaceW_ = 0, surfaceH_ = 0;
};

}  // namespace gles
}  // namespace opsu
