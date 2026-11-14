#pragma once
#include <string>
#include <vector>

namespace opsu {

/** UTF-8 to code points. Malformed bytes, overlong forms and surrogates become U+FFFD (one per bad byte run start). */
std::vector<char32_t> decodeUtf8(const std::string& s);

/** Horizontal advance of a glyph in pixels (float, like FreeType's 26.6 value / 64). */
class IAdvanceSource {
public:
    virtual ~IAdvanceSource() = default;
    virtual float advance(char32_t cp) = 0;
};

/** Pen position on the baseline of one glyph; the glyph quad is at (x + bitmapLeft, y - bitmapTop). */
struct PlacedGlyph {
    char32_t cp = 0;
    float x = 0.f, y = 0.f;
};

/**
 * DynamicFreeTypeFont.getWidth: the sum of the advances of every character (a '\n' counts too, with whatever
 * advance the font gives it), cast to int.
 */
int measureTextWidth(const std::vector<char32_t>& cps, IAdvanceSource& adv);

/**
 * DynamicFreeTypeFont.draw: x and y are truncated to int, the first baseline is y + ascent, '\n' moves the pen back
 * to the start x and down by lineHeight, every other character is placed at the pen and advances it.
 * No kerning (the Java has it commented out). Appends to `out`.
 */
void layoutText(const std::vector<char32_t>& cps, float x, float y, int ascent, int lineHeight, IAdvanceSource& adv,
                std::vector<PlacedGlyph>& out);

}  // namespace opsu
