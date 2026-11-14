#include "opsu/gfx/Image.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "opsu/gfx/Colors.h"
#include "opsu/gfx/Graphics.h"
#include "opsu/gfx/RenderTarget.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

namespace {
int roundJ(float v) { return static_cast<int>(std::floor(v + 0.5f)); }
}  // namespace

Image::Image() = default;

Image::Image(const std::string& filename) : Image(filename, 0.f) {}

Image::Image(const std::string& filename, float hueShift) : filename_(filename) {
    entry_ = ImageManager::instance().load(filename, hueShift);
    if (entry_) {
        region_ = entry_->region;
        width_ = static_cast<float>(entry_->logicalW);
        height_ = static_cast<float>(entry_->logicalH);
    }
}

void Image::retainEntry() { ImageManager::instance().retain(entry_); }

Image::Image(const Image& c, float w, float h) : region_(c.region_), width_(w), height_(h), entry_(c.entry_), rt_(c.rt_), filename_(c.filename_) {
    retainEntry();
}

Image::Image(const Image& c, int x, int y, int wid, int hei)
    : entry_(c.entry_), rt_(c.rt_), filename_(c.filename_) {
    retainEntry();
    if (c.width_ <= 0.f || c.height_ <= 0.f) return;
    // texels per size unit (differs from 1 when a huge file was scaled down to the texture cap)
    const float dx = static_cast<float>(c.region_.w) / c.width_;
    const float dy = static_cast<float>(c.region_.h) / c.height_;
    int left = roundJ(static_cast<float>(x) * dx), top = roundJ(static_cast<float>(y) * dy);
    const int tw = std::max(0, roundJ(static_cast<float>(wid) * dx)), th = std::max(0, roundJ(static_cast<float>(hei) * dy));
    // inside a flipped parent the rectangle counts from the other edge
    if (c.region_.flipX) left = c.region_.w - left - tw;
    if (c.region_.flipY) top = c.region_.h - top - th;
    region_ = c.region_.sub(left, top, tw, th);
    region_.flipX = c.region_.flipX;
    region_.flipY = c.region_.flipY;
    width_ = static_cast<float>(tw) / dx;
    height_ = static_cast<float>(th) / dy;
}

Image::Image(int width, int height) {
    rt_ = RenderTarget::create(width, height);
    if (rt_) {
        region_ = rt_->getRegion();
        width_ = static_cast<float>(width);
        height_ = static_cast<float>(height);
    }
}

Image::Image(Image&& o) noexcept
    : region_(std::move(o.region_)),
      width_(o.width_),
      height_(o.height_),
      entry_(std::move(o.entry_)),
      rt_(std::move(o.rt_)),
      filename_(std::move(o.filename_)),
      alpha_(o.alpha_),
      rotation_(o.rotation_),
      imageColor_(o.imageColor_),
      destroyed_(o.destroyed_),
      pixmap_(std::move(o.pixmap_)) {
    o.destroyed_ = true;  // nothing left to release
}

Image& Image::operator=(Image&& o) noexcept {
    if (this == &o) return *this;
    destroy();
    region_ = std::move(o.region_);
    width_ = o.width_;
    height_ = o.height_;
    entry_ = std::move(o.entry_);
    rt_ = std::move(o.rt_);
    filename_ = std::move(o.filename_);
    alpha_ = o.alpha_;
    rotation_ = o.rotation_;
    imageColor_ = o.imageColor_;
    destroyed_ = o.destroyed_;
    pixmap_ = std::move(o.pixmap_);
    o.destroyed_ = true;
    return *this;
}

Image::~Image() { destroy(); }

void Image::destroy() {
    if (!destroyed_) {
        if (entry_) ImageManager::instance().release(entry_);
        entry_.reset();
        pixmap_.reset();
        if (rt_) rt_->destroy();
        rt_.reset();
    }
    destroyed_ = true;
}

int Image::getWidth() const { return java::toInt(width_); }
int Image::getHeight() const { return java::toInt(height_); }

Image Image::getScaledCopy(float w, float h) const { return Image(*this, w, h); }
Image Image::getScaledCopy(float f) const { return Image(*this, width_ * f, height_ * f); }
Image Image::copy() const { return Image(*this, width_, height_); }
Image Image::getSubImage(int x, int y, int w, int h) const { return Image(*this, x, y, w, h); }

void Image::setAlpha(float f) { alpha_ = f < 0.f ? 0.f : (f > 1.f ? 1.f : f); }

TextureRegion Image::getTextureRegion() const { return region_; }

bool Image::usable() const { return !destroyed_ && (!entry_ || !entry_->freed); }

void Image::draw() { draw(0.f, 0.f, colors::white); }
void Image::draw(float x, float y) { draw(x, y, colors::white); }

void Image::draw(float x, float y, const Color& color) {
    Graphics& g = Graphics::getGraphics();
    g.setColorAlpha(color, alpha_);
    if (usable()) g.drawTexture(getTextureRegion(), x, y, static_cast<float>(getWidth()), static_cast<float>(getHeight()), rotation_);
}

void Image::drawCentered(float x, float y, const Color& color) {
    draw(x - static_cast<float>(getWidth()) / 2.f, y - static_cast<float>(getHeight()) / 2.f, color);
}
void Image::drawCentered(float x, float y) { drawCentered(x, y, colors::white); }

void Image::draw(float x, float y, float w, float h) { draw(x, y, w, h, colors::white); }

void Image::draw(float x, float y, float w, float h, const Color& color) {
    Graphics& g = Graphics::getGraphics();
    g.setColorAlpha(color, alpha_);
    if (usable()) g.drawTexture(getTextureRegion(), x, y, w, h, rotation_);
}

void Image::drawCentered(float x, float y, float scale, const Color& color) {
    const float w = width_ * scale, h = height_ * scale;
    draw(x - w / 2.f, y - h / 2.f, w, h, color);
}

void Image::drawCentered(float x, float y, float scale, float alphaMul, const Color& color) {
    const float w = width_ * scale, h = height_ * scale;
    Graphics& g = Graphics::getGraphics();
    g.setColorAlpha(color, alpha_ * alphaMul);
    if (usable()) g.drawTexture(getTextureRegion(), x - w / 2.f, y - h / 2.f, w, h, rotation_);
}

void Image::drawCentered(float x, float y, float scale, float rotationDeg, float alphaMul, const Color& color) {
    const float w = width_ * scale, h = height_ * scale;
    Graphics& g = Graphics::getGraphics();
    g.setColorAlpha(color, alpha_ * alphaMul);
    if (usable()) g.drawTexture(getTextureRegion(), x - w / 2.f, y - h / 2.f, w, h, rotationDeg);
}

void Image::setFlipped(bool flipX, bool flipY) {
    region_.flipX = flipX;
    region_.flipY = flipY;
}

void Image::drawEmbedded(float x, float y, int w, int h, float rotation) {
    Graphics& g = Graphics::getGraphics();
    g.setColor(imageColor_);
    if (usable()) g.drawTexture(getTextureRegion(), x, y, static_cast<float>(w), static_cast<float>(h), rotation);
}

void Image::drawEmbedded(float x, float y, float w, float h, int angle) {
    Graphics& g = Graphics::getGraphics();
    g.setColor(imageColor_);
    if (usable()) g.drawTexture(getTextureRegion(), x, y, w, h, static_cast<float>(angle));
}

bool Image::ensurePixmap() {
    if (pixmap_) return true;
    IRenderDevice* dev = renderDevice();
    if (!dev || filename_.empty()) return false;
    auto p = std::make_unique<Pixmap>();
    if (!dev->imageDecoder().decode(filename_, *p) || p->empty()) return false;
    pixmap_ = std::move(p);
    return true;
}

namespace {
const uint8_t* pixelAt(const Pixmap& p, int x, int y, float width, float height) {
    int px = width > 0.f ? java::toInt(static_cast<double>(x) * p.w / static_cast<double>(width)) : 0;
    int py = height > 0.f ? java::toInt(static_cast<double>(y) * p.h / static_cast<double>(height)) : 0;
    px = std::max(0, std::min(p.w - 1, px));
    py = std::max(0, std::min(p.h - 1, py));
    return &p.rgba[(static_cast<size_t>(py) * static_cast<size_t>(p.w) + static_cast<size_t>(px)) * 4u];
}
}  // namespace

float Image::getAlphaAt(int x, int y) {
    if (!ensurePixmap()) return 0.f;
    return static_cast<float>(pixelAt(*pixmap_, x, y, width_, height_)[3]);
}

Color Image::getColor(int x, int y) {
    if (!ensurePixmap()) return Color(0.f, 0.f, 0.f, 0.f);
    const uint8_t* p = pixelAt(*pixmap_, x, y, width_, height_);
    return Color(static_cast<int>(p[0]), static_cast<int>(p[1]), static_cast<int>(p[2]), static_cast<int>(p[3]));
}

Graphics& Image::getGraphics() {
    if (rt_) {
        if (auto g = rt_->getGraphics()) return *g;
    }
    throw std::logic_error("Getting graphics for non framebuffer image");
}

}  // namespace opsu
