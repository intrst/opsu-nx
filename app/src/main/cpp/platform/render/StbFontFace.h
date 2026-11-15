#pragma once
// IFontFace over stb_truetype, shared by the GLES and a later Vulkan backend. Android only (reads through AssetFiles).
#include <memory>
#include <string>
#include <vector>

#include "../../third_party/stb/stb_truetype.h"
#include "opsu/gfx/IRenderBackend.h"

namespace opsu {
namespace render {

/**
 * One TrueType file at one pixel size. The size is the EM size in pixels like FreeType's setPixelSizes(0, size)
 * (stbtt_ScaleForMappingEmToPixels), so line heights match the Java: ascent = ceil(hhea ascender * scale),
 * descent = floor(hhea descender * scale) (negative). Unhinted (the Java hints below 16 px; accepted difference).
 */
class StbFontFace : public IFontFace {
public:
    StbFontFace(std::shared_ptr<const std::vector<uint8_t>> data, float pixelSize);
    bool valid() const { return valid_; }

    bool hasGlyph(char32_t cp) const override;
    int ascent() const override { return ascent_; }
    int descent() const override { return descent_; }
    float advance(char32_t cp) override;
    bool rasterize(char32_t cp, GlyphBitmap& out) override;

private:
    std::shared_ptr<const std::vector<uint8_t>> data_;  // stbtt_fontinfo points into it
    stbtt_fontinfo info_;
    float scale_ = 0.f;
    int ascent_ = 0, descent_ = 0;
    bool valid_ = false;
};

/**
 * Loads `name` ("Exo2-Regular.ttf"): a name without '/' is looked up as asset "res/<name>" first, then as given
 * (absolute paths = file system). The file bytes are cached per name, so each size only costs the face.
 * null if the file is missing or not a font.
 */
std::shared_ptr<IFontFace> loadStbFontFace(const std::string& name, float pixelSize);
/** Drops the cached file bytes (faces already made keep theirs). */
void clearFontFileCache();

}  // namespace render
}  // namespace opsu
