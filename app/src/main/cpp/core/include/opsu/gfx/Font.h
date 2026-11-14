#pragma once
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "opsu/gfx/IRenderBackend.h"
#include "opsu/gfx/ShelfPacker.h"
#include "opsu/util/Color.h"

namespace opsu {

/** One glyph quad of a laid-out string: draw `region` with its top-left at (x, y), w x h pixels. */
struct GlyphQuad {
    TextureRegion region;
    float x = 0.f, y = 0.f;
    int w = 0, h = 0;
};

/**
 * Port of fluddokt.opsu.fake.Font + DynamicFreeTypeFont (FreeType replaced by IFontFace / stb_truetype).
 * Glyphs are rasterised on demand into 512 x 512 atlas pages (1 px padding, new pages as needed), white with the
 * coverage as alpha like the Java. A code point the main face lacks comes from the backup font (UnicodeFont.addBackupFont,
 * e.g. DroidSansFallback.ttf), or from the main face's "missing glyph" if there is no backup.
 * Metrics follow the Java: line height = ascent - descent, getHeight(str) = line height, width = (int) sum of advances,
 * no kerning. Strings are UTF-8 (the Java iterates UTF-16 units, so characters above U+FFFF were two missing glyphs there).
 */
class Font {
public:
    static constexpr int PLAIN = 0;
    static constexpr int BOLD = 1;

    explicit Font(const std::string& name);
    Font(const std::string& name, int style, float size);
    virtual ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    std::shared_ptr<Font> deriveFont(int style, float size) const;
    std::shared_ptr<Font> deriveFont(int style) const;
    std::shared_ptr<Font> deriveFont(float size) const;

    /** The fallback face source (UnicodeFont.addBackupFont). */
    void addBackupFont(std::shared_ptr<Font> backup);
    /** This font at `scale` times its size (cached, backup scaled too). Used for pixel-snapped text. */
    std::shared_ptr<Font> getScaledVariant(float scale);

    void addAsciiGlyphs() {}
    void addGlyphs(const std::string&) {}
    void loadGlyphs() {}

    /** Draws with the current Graphics (y + paddingTop, like Font.drawString). */
    void drawString(float x, float y, const std::string& s);
    void drawString(float x, float y, const std::string& s, const Color& textColor);

    int getHeight(const std::string& s);
    int getLineHeight();
    int getWidth(const std::string& s);

    void setPaddingBottom(int p) { padBottom_ = p; }
    void setPaddingTop(int p) { padTop_ = p; }
    int getPaddingTop() const { return padTop_; }
    int getPaddingBottom() const { return padBottom_; }

    const std::string& getName() const { return name_; }
    int getStyle() const { return style_; }
    float getSize() const { return size_; }

    /** Frees the glyph pages; the font stays usable (new pages are made on the next draw). */
    void destroy();
    static void destroyAll();

    /** DynamicFreeTypeFont.draw layout: the quads of `s` with the pen starting at (x, y) (top of the first line). Rasterises what is missing. */
    void layoutGlyphs(const std::string& s, float x, float y, std::vector<GlyphQuad>& out);

    /** For tests/diagnostics. */
    int pageCount() const { return static_cast<int>(pages_.size()); }

private:
    struct Page {
        std::shared_ptr<ITexture> tex;
        std::shared_ptr<std::vector<uint8_t>> pixels;  // CPU copy, kept to refill the texture after a lost context
    };
    struct Glyph {
        TextureRegion region;  // tex == nullptr: nothing to draw (blank glyph)
        float advance = 0.f;
        int left = 0, top = 0, w = 0, h = 0;
    };
    class AdvanceAdapter;

    Font(const std::string& name, int style, float size, bool registerInList);
    bool ensureFace();
    /** Which face/font renders cp: this one, or the backup. */
    Font* sourceFor(char32_t cp);
    float advanceOf(char32_t cp);
    const Glyph& glyphOf(char32_t cp);
    void rasterizeInto(Font& src, char32_t cp, Glyph& g);
    int ascent();
    int descent();

    std::string name_;
    int style_;
    float size_;
    int padTop_ = 0, padBottom_ = 0;
    std::shared_ptr<IFontFace> face_;
    bool faceTried_ = false;
    std::shared_ptr<Font> backup_;
    std::unordered_map<char32_t, float> advances_;
    std::unordered_map<char32_t, Glyph> glyphs_;
    std::vector<Page> pages_;
    ShelfPacker packer_{512, 512, 1};
    std::map<int, std::shared_ptr<Font>> scaled_;
};

/** fluddokt.opsu.fake.UnicodeFont: a Font copied from another one. */
class UnicodeFont : public Font {
public:
    explicit UnicodeFont(const Font& font) : Font(font.getName(), font.getStyle(), font.getSize()) {}
    void addGlyphs(char32_t, char32_t) {}
    using Font::addGlyphs;
};

}  // namespace opsu
