#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace opsu {

enum class TextureFilter { Nearest, Linear };

/** CPU pixels, RGBA8, row 0 = top (what stb_image returns). Not premultiplied. */
struct Pixmap {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    Pixmap() = default;
    Pixmap(int w_, int h_) : w(w_), h(h_), rgba(static_cast<size_t>(w_) * static_cast<size_t>(h_) * 4u, 0) {}
    bool empty() const { return w <= 0 || h <= 0; }
};

/** A GPU texture. Backend-specific object, no GL/Vulkan types here. Used on the render thread only. */
class ITexture {
public:
    virtual ~ITexture() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void setFilter(TextureFilter minFilter, TextureFilter magFilter) = 0;
    /** Overwrites a sub-rectangle with RGBA8 pixels (row 0 = top row of the rectangle). */
    virtual void update(int x, int y, int w, int h, const uint8_t* rgba) = 0;
    /**
     * True if texel row 0 is the BOTTOM of the picture (GL framebuffer textures: the game's y-down drawing ends up
     * with its top at the last row). Loaded images and glyph pages return false.
     */
    virtual bool rowZeroIsBottom() const { return false; }
};

/**
 * A rectangle of a texture in texels (x, y from the picture's top-left), plus flip flags.
 * Port of libGDX's TextureRegion as Image.java uses it; Image.java's flip(false, true) to cope with the y-down
 * camera is not needed here because the UVs are computed top-down.
 */
struct TextureRegion {
    std::shared_ptr<ITexture> tex;
    int x = 0, y = 0, w = 0, h = 0;
    bool flipX = false, flipY = false;

    /** UV of the top-left (u0, v0) and bottom-right (u1, v1) of the visible picture, after the flip flags. */
    void uvs(float& u0, float& v0, float& u1, float& v1) const;
    /** A region inside this one (rx, ry relative to this region's top-left, flips ignored). */
    TextureRegion sub(int rx, int ry, int rw, int rh) const;
};

}  // namespace opsu
