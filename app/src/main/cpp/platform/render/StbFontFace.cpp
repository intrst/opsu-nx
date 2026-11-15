#include "StbFontFace.h"

#include <android/log.h>

#include <cmath>
#include <map>

#include "AssetFiles.h"

#define LOG_TAG "opsu-render"

namespace opsu {
namespace render {

StbFontFace::StbFontFace(std::shared_ptr<const std::vector<uint8_t>> data, float pixelSize) : data_(std::move(data)) {
    if (!data_ || data_->empty() || pixelSize <= 0.f) return;
    const unsigned char* p = data_->data();
    const int off = stbtt_GetFontOffsetForIndex(p, 0);
    if (off < 0 || !stbtt_InitFont(&info_, p, off)) return;
    scale_ = stbtt_ScaleForMappingEmToPixels(&info_, pixelSize);
    int asc = 0, desc = 0, gap = 0;
    stbtt_GetFontVMetrics(&info_, &asc, &desc, &gap);
    ascent_ = static_cast<int>(std::ceil(asc * scale_));
    descent_ = static_cast<int>(std::floor(desc * scale_));
    valid_ = true;
}

bool StbFontFace::hasGlyph(char32_t cp) const { return valid_ && stbtt_FindGlyphIndex(&info_, static_cast<int>(cp)) != 0; }

float StbFontFace::advance(char32_t cp) {
    if (!valid_) return 0.f;
    const int g = stbtt_FindGlyphIndex(&info_, static_cast<int>(cp));  // 0 = the font's "missing" glyph
    int adv = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&info_, g, &adv, &lsb);
    return adv * scale_;
}

bool StbFontFace::rasterize(char32_t cp, GlyphBitmap& out) {
    if (!valid_) return false;
    const int g = stbtt_FindGlyphIndex(&info_, static_cast<int>(cp));
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&info_, g, scale_, scale_, &x0, &y0, &x1, &y1);
    out.width = x1 - x0;
    out.height = y1 - y0;
    out.left = x0;
    out.top = -y0;  // stb: y down from the baseline; FreeType bitmap_top: up
    out.coverage.clear();
    if (out.width <= 0 || out.height <= 0) { out.width = out.height = 0; return true; }  // blank glyph
    out.coverage.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height), 0);
    stbtt_MakeGlyphBitmap(&info_, out.coverage.data(), out.width, out.height, out.width, scale_, scale_, g);
    return true;
}

namespace {
std::map<std::string, std::shared_ptr<const std::vector<uint8_t>>>& cache() {
    static std::map<std::string, std::shared_ptr<const std::vector<uint8_t>>> c;
    return c;
}
}  // namespace

std::shared_ptr<IFontFace> loadStbFontFace(const std::string& name, float pixelSize) {
    auto it = cache().find(name);
    if (it == cache().end()) {
        auto bytes = std::make_shared<std::vector<uint8_t>>();
        bool ok = false;
        if (name.find('/') == std::string::npos) ok = readFileOrAsset("res/" + name, *bytes);
        if (!ok) ok = readFileOrAsset(name, *bytes);
        if (!ok) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "font not found: %s", name.c_str());
            return nullptr;
        }
        it = cache().emplace(name, std::move(bytes)).first;
    }
    auto face = std::make_shared<StbFontFace>(it->second, pixelSize);
    if (!face->valid()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "not a usable TrueType font: %s", name.c_str());
        return nullptr;
    }
    return face;
}

void clearFontFileCache() { cache().clear(); }

}  // namespace render
}  // namespace opsu
