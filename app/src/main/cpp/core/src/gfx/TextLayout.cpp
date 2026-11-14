#include "opsu/gfx/TextLayout.h"

#include "opsu/util/JavaCompat.h"

namespace opsu {

std::vector<char32_t> decodeUtf8(const std::string& s) {
    std::vector<char32_t> out;
    out.reserve(s.size());
    const size_t n = s.size();
    size_t i = 0;
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { out.push_back(c); i++; continue; }
        int len = 0;
        char32_t cp = 0, minCp = 0;
        if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1F; minCp = 0x80; }
        else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0F; minCp = 0x800; }
        else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07; minCp = 0x10000; }
        bool ok = len > 0 && i + static_cast<size_t>(len) <= n;
        for (int k = 1; ok && k < len; k++) {
            const unsigned char cc = static_cast<unsigned char>(s[i + static_cast<size_t>(k)]);
            if ((cc & 0xC0) != 0x80) ok = false;
            else cp = (cp << 6) | (cc & 0x3F);
        }
        if (ok && (cp < minCp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))) ok = false;
        if (ok) { out.push_back(cp); i += static_cast<size_t>(len); }
        else { out.push_back(0xFFFD); i++; }
    }
    return out;
}

int measureTextWidth(const std::vector<char32_t>& cps, IAdvanceSource& adv) {
    float len = 0.f;
    for (char32_t cp : cps) len += adv.advance(cp);
    return java::toInt(len);
}

void layoutText(const std::vector<char32_t>& cps, float x, float y, int ascent, int lineHeight, IAdvanceSource& adv,
                std::vector<PlacedGlyph>& out) {
    float px = static_cast<float>(java::toInt(x));
    float py = static_cast<float>(java::toInt(y));
    const float ox = px;
    py += static_cast<float>(ascent);
    for (char32_t cp : cps) {
        if (cp == U'\n') {
            py += static_cast<float>(lineHeight);
            px = ox;
        } else {
            out.push_back(PlacedGlyph{cp, px, py});
            px += adv.advance(cp);
        }
    }
}

}  // namespace opsu
