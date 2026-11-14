#include "opsu/gfx/ImageManager.h"

#include <algorithm>
#include <cmath>

#include "opsu/util/JavaCompat.h"

namespace opsu {

namespace {
constexpr int kAtlasSize = 512;
int roundJ(float v) { return static_cast<int>(std::floor(v + 0.5f)); }  // Math.round(float)
}  // namespace

ImageManager& ImageManager::instance() {
    static ImageManager m;
    return m;
}

int ImageManager::textureCap() {
    if (texCap_ < 0) {
        int sw = 0, sh = 0, gpu = 2048;
        if (IRenderDevice* d = renderDevice()) { d->screenSize(sw, sh); gpu = d->maxTextureSize(); }
        // never below 1600, never above 2048 (phone GPU/memory limit); same rule as the Java
        texCap_ = std::min(2048, std::max(1600, std::max(sw, sh)));
        texCap_ = std::min(texCap_, std::max(64, gpu));
    }
    return texCap_;
}

void ImageManager::shiftHue(Pixmap& p, float degrees) {
    for (int y = 0; y < p.h; y++) {
        for (int x = 0; x < p.w; x++) {
            uint8_t* px = &p.rgba[(static_cast<size_t>(y) * static_cast<size_t>(p.w) + static_cast<size_t>(x)) * 4u];
            const int a = px[3];
            if (a == 0) continue;
            const float r = px[0] / 255.f, g = px[1] / 255.f, b = px[2] / 255.f;
            const float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
            const float d = mx - mn;
            if (d < 0.004f) continue;  // grey: no hue
            float hue;
            if (mx == r) hue = std::fmod((g - b) / d, 6.f);
            else if (mx == g) hue = (b - r) / d + 2.f;
            else hue = (r - g) / d + 4.f;
            hue = std::fmod(std::fmod(hue * 60.f + degrees, 360.f) + 360.f, 360.f);
            const float s = d / mx, v = mx;
            const float c = v * s, hp = hue / 60.f;
            const float xx = c * (1.f - std::fabs(std::fmod(hp, 2.f) - 1.f));
            float r1, g1, b1;
            if (hp < 1.f) { r1 = c; g1 = xx; b1 = 0.f; }
            else if (hp < 2.f) { r1 = xx; g1 = c; b1 = 0.f; }
            else if (hp < 3.f) { r1 = 0.f; g1 = c; b1 = xx; }
            else if (hp < 4.f) { r1 = 0.f; g1 = xx; b1 = c; }
            else if (hp < 5.f) { r1 = xx; g1 = 0.f; b1 = c; }
            else { r1 = c; g1 = 0.f; b1 = xx; }
            const float m = v - c;
            px[0] = static_cast<uint8_t>(roundJ((r1 + m) * 255.f));
            px[1] = static_cast<uint8_t>(roundJ((g1 + m) * 255.f));
            px[2] = static_cast<uint8_t>(roundJ((b1 + m) * 255.f));
        }
    }
}

Pixmap ImageManager::capPixmap(Pixmap p, int cap) {
    const int longest = std::max(p.w, p.h);
    if (longest <= cap) return p;
    const float f = static_cast<float>(cap) / static_cast<float>(longest);
    const int nw = std::max(1, roundJ(static_cast<float>(p.w) * f)), nh = std::max(1, roundJ(static_cast<float>(p.h) * f));
    Pixmap d(nw, nh);
    const double fx = static_cast<double>(p.w) / nw, fy = static_cast<double>(p.h) / nh;
    for (int y = 0; y < nh; y++) {
        const double y0 = y * fy, y1 = (y + 1) * fy;
        for (int x = 0; x < nw; x++) {
            const double x0 = x * fx, x1 = (x + 1) * fx;
            double acc[4] = {0, 0, 0, 0}, wsum = 0;  // colour weighted by alpha, so transparent pixels do not darken the edge
            for (int sy = static_cast<int>(y0); sy < p.h && sy < std::ceil(y1); sy++) {
                const double wy = std::min(y1, sy + 1.0) - std::max(y0, static_cast<double>(sy));
                for (int sx = static_cast<int>(x0); sx < p.w && sx < std::ceil(x1); sx++) {
                    const double wx = std::min(x1, sx + 1.0) - std::max(x0, static_cast<double>(sx));
                    const double w = wx * wy;
                    const uint8_t* s = &p.rgba[(static_cast<size_t>(sy) * static_cast<size_t>(p.w) + static_cast<size_t>(sx)) * 4u];
                    acc[0] += s[0] * s[3] * w; acc[1] += s[1] * s[3] * w; acc[2] += s[2] * s[3] * w;
                    acc[3] += s[3] * w;
                    wsum += w;
                }
            }
            uint8_t* o = &d.rgba[(static_cast<size_t>(y) * static_cast<size_t>(nw) + static_cast<size_t>(x)) * 4u];
            if (wsum > 0 && acc[3] > 0) {
                o[0] = static_cast<uint8_t>(std::min(255.0, std::floor(acc[0] / acc[3] + 0.5)));
                o[1] = static_cast<uint8_t>(std::min(255.0, std::floor(acc[1] / acc[3] + 0.5)));
                o[2] = static_cast<uint8_t>(std::min(255.0, std::floor(acc[2] / acc[3] + 0.5)));
                o[3] = static_cast<uint8_t>(std::min(255.0, std::floor(acc[3] / wsum + 0.5)));
            }
        }
    }
    return d;
}

std::shared_ptr<ImageManager::Entry> ImageManager::load(const std::string& path, float hueShift) {
    const std::string key = hueShift == 0.f ? path : path + "#hue" + std::to_string(roundJ(hueShift * 10.f));
    auto it = map_.find(key);
    if (it != map_.end()) { it->second->refcnt++; return it->second; }
    IRenderDevice* dev = renderDevice();
    if (!dev) return nullptr;
    Pixmap p;
    if (!dev->imageDecoder().decode(path, p) || p.empty()) return nullptr;  // Java: blank image
    auto e = std::make_shared<Entry>();
    e->key = key;
    e->path = path;
    e->hueShift = hueShift;
    e->logicalW = p.w;
    e->logicalH = p.h;
    if (hueShift != 0.f) shiftHue(p, hueShift);
    p = capPixmap(std::move(p), textureCap());
    place(e, p);
    if (!e->region.tex) return nullptr;
    e->refcnt = 1;
    map_[key] = e;
    return e;
}

void ImageManager::place(const std::shared_ptr<Entry>& e, const Pixmap& p) {
    IRenderDevice* dev = renderDevice();
    auto atlas = std::make_shared<Atlas>();
    int x = 0, y = 0;
    if (p.w >= kAtlasSize || p.h >= kAtlasSize) {
        // its own texture (a restorer re-decodes the file after a lost context)
        atlas->w = p.w; atlas->h = p.h; atlas->full = true;
        std::weak_ptr<Atlas> wa = atlas;
        atlas->tex = dev->createTexture(p.w, p.h, p.rgba.data(), TextureFilter::Linear,
                                        [wa](ITexture&) { if (auto a = wa.lock()) ImageManager::instance().restoreAtlas(*a); });
        atlases_.push_back(atlas);
    } else {
        ShelfPacker::Slot slot;
        if (!packer_.pack(p.w, p.h, slot)) return;
        if (slot.newPage || !current_) {
            if (current_) { current_->full = true; current_->cpu.reset(); }
            current_ = std::make_shared<Atlas>();
            current_->w = kAtlasSize; current_->h = kAtlasSize;
            current_->cpu = std::make_shared<Pixmap>(kAtlasSize, kAtlasSize);
            atlases_.push_back(current_);
            // texture created after the first image is copied in, see below
        }
        atlas = current_;
        x = slot.x; y = slot.y;
        for (int row = 0; row < p.h; row++) {
            std::copy(p.rgba.begin() + static_cast<ptrdiff_t>(row) * p.w * 4, p.rgba.begin() + static_cast<ptrdiff_t>(row + 1) * p.w * 4,
                      atlas->cpu->rgba.begin() + (static_cast<ptrdiff_t>(y + row) * kAtlasSize + x) * 4);
        }
        if (!atlas->tex) {
            std::weak_ptr<Atlas> wa = atlas;
            atlas->tex = dev->createTexture(kAtlasSize, kAtlasSize, atlas->cpu->rgba.data(), TextureFilter::Linear,
                                            [wa](ITexture&) { if (auto a = wa.lock()) ImageManager::instance().restoreAtlas(*a); });
        } else {
            atlas->tex->update(x, y, p.w, p.h, p.rgba.data());
        }
    }
    e->atlas = atlas;
    e->region.tex = atlas->tex;
    e->region.x = x; e->region.y = y; e->region.w = p.w; e->region.h = p.h;
    atlas->entries.push_back(e);
}

void ImageManager::restoreAtlas(Atlas& a) {
    if (!a.tex) return;
    if (a.cpu) { a.tex->update(0, 0, a.w, a.h, a.cpu->rgba.data()); return; }
    IRenderDevice* dev = renderDevice();
    if (!dev) return;
    for (const auto& e : a.entries) {
        Pixmap p;
        if (!dev->imageDecoder().decode(e->path, p) || p.empty()) continue;
        if (e->hueShift != 0.f) shiftHue(p, e->hueShift);
        p = capPixmap(std::move(p), textureCap());  // same size as when it was first loaded (the cap is fixed)
        if (p.w != e->region.w || p.h != e->region.h) continue;
        a.tex->update(e->region.x, e->region.y, p.w, p.h, p.rgba.data());
    }
}

void ImageManager::retain(const std::shared_ptr<Entry>& e) {
    if (e) e->refcnt++;
}

void ImageManager::dropIfEmpty(const std::shared_ptr<Atlas>& a) {
    if (a && a->full && a->entries.empty()) {
        atlases_.erase(std::remove(atlases_.begin(), atlases_.end(), a), atlases_.end());
    }
}

void ImageManager::release(const std::shared_ptr<Entry>& e) {
    if (!e || e->freed) return;
    if (--e->refcnt > 0) return;
    e->freed = true;
    map_.erase(e->key);
    if (auto a = e->atlas.lock()) {
        auto& v = a->entries;
        v.erase(std::remove(v.begin(), v.end(), e), v.end());
        dropIfEmpty(a);
    }
}

void ImageManager::clearAll() {
    for (auto& kv : map_) kv.second->freed = true;
    map_.clear();
    atlases_.clear();
    current_.reset();
    packer_.reset();
}

}  // namespace opsu
