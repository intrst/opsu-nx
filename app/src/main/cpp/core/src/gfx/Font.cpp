#include "opsu/gfx/Font.h"

#include <algorithm>
#include <cmath>

#include "opsu/gfx/Graphics.h"
#include "opsu/gfx/TextLayout.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

namespace {
std::vector<Font*>& allFonts() {
    static std::vector<Font*> v;
    return v;
}
constexpr int kPage = 512;
}  // namespace

class Font::AdvanceAdapter : public IAdvanceSource {
public:
    explicit AdvanceAdapter(Font& f) : f_(f) {}
    float advance(char32_t cp) override { return f_.advanceOf(cp); }

private:
    Font& f_;
};

Font::Font(const std::string& name) : Font(name, PLAIN, 12.f, true) {}
Font::Font(const std::string& name, int style, float size) : Font(name, style, size, true) {}
Font::Font(const std::string& name, int style, float size, bool registerInList)
    : name_(name), style_(style), size_(size) {
    if (registerInList) allFonts().push_back(this);
}

Font::~Font() {
    auto& v = allFonts();
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

std::shared_ptr<Font> Font::deriveFont(int style, float size) const { return std::make_shared<Font>(name_, style, size); }
std::shared_ptr<Font> Font::deriveFont(int style) const { return std::make_shared<Font>(name_, style, size_); }
std::shared_ptr<Font> Font::deriveFont(float size) const { return std::make_shared<Font>(name_, style_, size); }

void Font::addBackupFont(std::shared_ptr<Font> backup) {
    backup_ = std::move(backup);
    glyphs_.clear();  // glyphs already cached from the main face's "missing" glyph must be looked up again
    advances_.clear();
}

std::shared_ptr<Font> Font::getScaledVariant(float scale) {
    const int key = static_cast<int>(std::floor(scale * 100.f + 0.5f));
    auto it = scaled_.find(key);
    if (it != scaled_.end()) return it->second;
    auto v = std::make_shared<Font>(name_, style_, size_ * (static_cast<float>(key) / 100.f));
    if (backup_) v->addBackupFont(backup_->getScaledVariant(scale));
    scaled_[key] = v;
    return v;
}

bool Font::ensureFace() {
    if (face_) return true;
    if (faceTried_) return false;
    faceTried_ = true;
    IRenderDevice* dev = renderDevice();
    if (!dev) { faceTried_ = false; return false; }  // no device yet: try again later
    // FreeType: face.setPixelSizes(0, Math.round(size))
    face_ = dev->loadFontFace(name_, static_cast<float>(std::floor(size_ + 0.5f)));
    return face_ != nullptr;
}

int Font::ascent() { return ensureFace() ? face_->ascent() : 0; }
int Font::descent() { return ensureFace() ? face_->descent() : 0; }

Font* Font::sourceFor(char32_t cp) {
    if (!ensureFace()) return nullptr;
    if (face_->hasGlyph(cp)) return this;
    if (backup_ && backup_->ensureFace()) return backup_.get();
    return this;  // no usable backup: the main face's missing glyph
}

float Font::advanceOf(char32_t cp) {
    auto it = advances_.find(cp);
    if (it != advances_.end()) return it->second;
    Font* src = sourceFor(cp);
    const float a = src ? src->face_->advance(cp) : 0.f;
    advances_[cp] = a;
    return a;
}

void Font::rasterizeInto(Font& src, char32_t cp, Glyph& g) {
    GlyphBitmap bmp;
    if (!src.face_->rasterize(cp, bmp)) return;
    g.left = bmp.left;
    g.top = bmp.top;
    g.w = bmp.width;
    g.h = bmp.height;
    if (bmp.width <= 0 || bmp.height <= 0 ||
        bmp.coverage.size() < static_cast<size_t>(bmp.width) * static_cast<size_t>(bmp.height))
        return;  // a blank glyph (space): advance only
    ShelfPacker::Slot slot;
    if (!packer_.pack(bmp.width, bmp.height, slot)) return;  // larger than a page: skip the glyph
    IRenderDevice* dev = renderDevice();
    if (!dev) return;
    if (slot.newPage) {
        Page p;
        p.pixels = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(kPage) * kPage * 4u, uint8_t{0});
        auto px = p.pixels;
        p.tex = dev->createTexture(kPage, kPage, px->data(), TextureFilter::Linear,
                                   [px](ITexture& t) { t.update(0, 0, kPage, kPage, px->data()); });
        pages_.push_back(std::move(p));
    }
    Page& page = pages_[static_cast<size_t>(slot.page)];
    const bool bold = (src.style_ & BOLD) != 0;
    std::vector<uint8_t> rgba(static_cast<size_t>(bmp.width) * static_cast<size_t>(bmp.height) * 4u);
    for (int y = 0; y < bmp.height; y++) {
        for (int x = 0; x < bmp.width; x++) {
            const size_t i = static_cast<size_t>(y) * static_cast<size_t>(bmp.width) + static_cast<size_t>(x);
            float a = bmp.coverage[i] / 255.f;
            if (bold) {  // Java: the glyph is drawn twice more with source-over onto itself
                for (int k = 0; k < 2; k++) {
                    const float c = bmp.coverage[i] / 255.f;
                    a = a + c * (1.f - a);
                }
            }
            uint8_t* d = &rgba[i * 4u];
            d[0] = d[1] = d[2] = 255;  // 0xffffff00 | coverage
            d[3] = static_cast<uint8_t>(std::min(255.f, std::floor(a * 255.f + 0.5f)));
        }
    }
    // keep the CPU copy of the page current (context loss) and upload only the new rectangle
    for (int y = 0; y < bmp.height; y++) {
        std::copy(rgba.begin() + static_cast<ptrdiff_t>(y) * bmp.width * 4,
                  rgba.begin() + static_cast<ptrdiff_t>(y + 1) * bmp.width * 4,
                  page.pixels->begin() + (static_cast<ptrdiff_t>(slot.y + y) * kPage + slot.x) * 4);
    }
    page.tex->update(slot.x, slot.y, bmp.width, bmp.height, rgba.data());
    g.region.tex = page.tex;
    g.region.x = slot.x;
    g.region.y = slot.y;
    g.region.w = bmp.width;
    g.region.h = bmp.height;
}

const Font::Glyph& Font::glyphOf(char32_t cp) {
    auto it = glyphs_.find(cp);
    if (it != glyphs_.end()) return it->second;
    Glyph g;
    Font* src = sourceFor(cp);
    if (src) {
        g.advance = advanceOf(cp);
        rasterizeInto(*src, cp, g);
    }
    return glyphs_.emplace(cp, std::move(g)).first->second;
}

void Font::layoutGlyphs(const std::string& s, float x, float y, std::vector<GlyphQuad>& out) {
    if (!ensureFace()) return;
    const std::vector<char32_t> cps = decodeUtf8(s);
    std::vector<PlacedGlyph> placed;
    AdvanceAdapter adv(*this);
    layoutText(cps, x, y, ascent(), getLineHeight() - padTop_ - padBottom_, adv, placed);
    for (const PlacedGlyph& p : placed) {
        const Glyph& g = glyphOf(p.cp);
        if (!g.region.tex) continue;
        GlyphQuad q;
        q.region = g.region;
        q.x = p.x + static_cast<float>(g.left);
        q.y = p.y - static_cast<float>(g.top);
        q.w = g.w;
        q.h = g.h;
        out.push_back(std::move(q));
    }
}

void Font::drawString(float x, float y, const std::string& s) {
    Graphics::getGraphics().drawString(*this, s, x, y + static_cast<float>(padTop_));
}

void Font::drawString(float x, float y, const std::string& s, const Color& textColor) {
    Graphics::getGraphics().setColor(textColor);
    drawString(x, y, s);
}

int Font::getLineHeight() { return ascent() - descent() + padTop_ + padBottom_; }
int Font::getHeight(const std::string&) { return getLineHeight(); }

int Font::getWidth(const std::string& s) {
    if (s.empty()) return 0;
    AdvanceAdapter adv(*this);
    return measureTextWidth(decodeUtf8(s), adv);
}

void Font::destroy() {
    pages_.clear();
    glyphs_.clear();
    advances_.clear();
    packer_.reset();
}

void Font::destroyAll() {
    // copy: destroy() does not change the list, but be safe against fonts made meanwhile
    const std::vector<Font*> list = allFonts();
    for (Font* f : list) f->destroy();
}

}  // namespace opsu
