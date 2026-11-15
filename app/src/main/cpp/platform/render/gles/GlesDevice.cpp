#include "GlesDevice.h"

#include <algorithm>

#include "../StbFontFace.h"

#define LOG_TAG "opsu-gles"

namespace opsu {
namespace gles {

// ---- GlesTexture ------------------------------------------------------------------------------------------------

GlesTexture::GlesTexture(GlesDevice& dev, int w, int h, bool bottom, TextureFilter f)
    : dev_(dev), w_(w), h_(h), bottom_(bottom), minF_(f), magF_(f) {}

GlesTexture::~GlesTexture() {
    if (id_ && gen_ == dev_.generation() && dev_.glValid()) {
        dev_.gl().textureDeleted(id_);
        glDeleteTextures(1, &id_);
    }
}

GLuint GlesTexture::glName() const { return (id_ && gen_ == dev_.generation() && dev_.glValid()) ? id_ : 0; }

void GlesTexture::applyFilter() {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minF_ == TextureFilter::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magF_ == TextureFilter::Linear ? GL_LINEAR : GL_NEAREST);
}

bool GlesTexture::createGl(const uint8_t* rgba) {
    if (!dev_.glValid()) return false;
    if (id_ && gen_ == dev_.generation()) {
        dev_.gl().textureDeleted(id_);
        glDeleteTextures(1, &id_);
    }
    glGenTextures(1, &id_);
    gen_ = dev_.generation();
    dev_.gl().bindTexture(id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    applyFilter();
    glCheck("createTexture");
    return true;
}

void GlesTexture::setPending(const uint8_t* rgba) {
    if (rgba) pending_.assign(rgba, rgba + static_cast<size_t>(w_) * static_cast<size_t>(h_) * 4u);
}

void GlesTexture::restore() {
    createGl(pending_.empty() ? nullptr : pending_.data());
    pending_.clear();
    pending_.shrink_to_fit();
    if (restore_) restore_(*this);
}

void GlesTexture::setFilter(TextureFilter minF, TextureFilter magF) {
    minF_ = minF;
    magF_ = magF;
    if (GLuint id = glName()) {
        dev_.gl().bindTexture(id);
        applyFilter();
    }
}

void GlesTexture::update(int x, int y, int w, int h, const uint8_t* rgba) {
    GLuint id = glName();
    if (!id || !rgba || w <= 0 || h <= 0) return;  // lost: the owner refills it through the restorer
    dev_.gl().bindTexture(id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glCheck("texSubImage");
}

// ---- GlesRenderTarget -------------------------------------------------------------------------------------------

GlesRenderTarget::GlesRenderTarget(GlesDevice& dev, int w, int h)
    : dev_(dev), w_(w), h_(h), tex_(std::make_shared<GlesTexture>(dev, w, h, true, TextureFilter::Linear)) {}

GlesRenderTarget::~GlesRenderTarget() {
    if (fbo_ && gen_ == dev_.generation() && dev_.glValid()) glDeleteFramebuffers(1, &fbo_);
}

GLuint GlesRenderTarget::fboName() const { return (fbo_ && gen_ == dev_.generation() && dev_.glValid()) ? fbo_ : 0; }

bool GlesRenderTarget::createGl() {
    if (!dev_.glValid()) return false;
    if (!tex_->glName() && !tex_->createGl(nullptr)) return false;
    if (fbo_ && gen_ == dev_.generation()) glDeleteFramebuffers(1, &fbo_);
    glGenFramebuffers(1, &fbo_);
    gen_ = dev_.generation();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_->glName(), 0);
    const GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    const bool ok = st == GL_FRAMEBUFFER_COMPLETE;
    if (!ok) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "FBO %dx%d incomplete: 0x%x", w_, h_, static_cast<unsigned>(st));
    else {
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    dev_.gl().rebindTarget();  // back to what the backend thinks is current (also its scissor/mask)
    return ok;
}

// ---- GlesDevice -------------------------------------------------------------------------------------------------

GlesDevice::GlesDevice() : backend_(*this) {}

template <class T>
void GlesDevice::prune(std::vector<std::weak_ptr<T>>& v) {
    v.erase(std::remove_if(v.begin(), v.end(), [](const std::weak_ptr<T>& w) { return w.expired(); }), v.end());
}

std::shared_ptr<ITexture> GlesDevice::createTexture(int w, int h, const uint8_t* rgba, TextureFilter filter,
                                                    TextureRestorer restore) {
    if (w < 1 || h < 1) return nullptr;
    auto t = std::make_shared<GlesTexture>(*this, w, h, false, filter);
    t->setRestorer(std::move(restore));
    if (valid_) t->createGl(rgba);
    else t->setPending(rgba);
    if (textures_.size() > 256) prune(textures_);
    textures_.push_back(t);
    return t;
}

std::shared_ptr<IRenderTarget> GlesDevice::createRenderTarget(int w, int h) {
    if (w < 1 || h < 1) return nullptr;
    auto rt = std::make_shared<GlesRenderTarget>(*this, w, h);
    if (valid_ && !rt->createGl()) return nullptr;
    targets_.push_back(rt);
    textures_.push_back(std::static_pointer_cast<GlesTexture>(rt->texture()));
    return rt;
}

std::shared_ptr<IFontFace> GlesDevice::loadFontFace(const std::string& name, float pixelSize) {
    return render::loadStbFontFace(name, pixelSize);
}

bool GlesDevice::onContextCreated() {
    gen_++;
    valid_ = true;
    if (!backend_.initGl()) { valid_ = false; return false; }
    GLint mt = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mt);
    if (mt > 0) maxTex_ = mt;
    prune(textures_);
    prune(targets_);
    int restored = 0;
    // textures first (atlases, glyph pages, files re-decoded by their owners), then the FBOs that use render textures
    for (auto& w : textures_)
        if (auto t = w.lock()) {
            if (t->rowZeroIsBottom()) continue;  // render target colour textures are made by their targets
            t->restore();
            restored++;
        }
    for (auto& w : targets_)
        if (auto rt = w.lock()) rt->createGl();
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "context created (generation %u): %d textures restored, %zu targets, max texture %d",
                        gen_, restored, targets_.size(), maxTex_);
    return true;
}

void GlesDevice::onContextLost() {
    if (!valid_) return;
    valid_ = false;
    gen_++;  // every GL name made so far is dead now
    backend_.loseGl();
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "context lost");
}

}  // namespace gles
}  // namespace opsu
