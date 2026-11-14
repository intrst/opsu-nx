#pragma once
// The low-level contract between the pure graphics code (Graphics, Image, Font) and a backend (GLES now, Vulkan
// later). No GL/EGL/Vulkan/Android types anywhere in here. Everything is called on the one render thread.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "opsu/gfx/DrawMode.h"
#include "opsu/gfx/Texture.h"
#include "opsu/gfx/ViewportMap.h"
#include "opsu/util/Color.h"

namespace opsu {

/** One vertex of a triangle list, already in the game's virtual coordinates (the Graphics transform is applied on the CPU). */
struct Vertex {
    float x, y;       ///< virtual coordinates, y down
    float u, v;       ///< texture coordinates (ignored when the draw has no texture)
    uint8_t r, g, b, a;  ///< non-premultiplied vertex colour, multiplied with the texel
};
static_assert(sizeof(Vertex) == 20, "Vertex is expected to be tightly packed");

/** An offscreen colour target (FBO). Its contents do not survive a lost graphics context. */
class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    /** The colour attachment, usable as a texture (its rowZeroIsBottom() is set accordingly). */
    virtual std::shared_ptr<ITexture> texture() = 0;
};

/**
 * Submits geometry. A backend batches drawTriangles calls and MUST submit the pending batch before it applies any
 * state change below (draw mode, scissor, target, clear) or when the texture changes, so the result is exactly what
 * immediate drawing in call order would give. "Never one draw call per sprite" is the backend's job.
 */
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    /** A triangle list (vertexCount % 3 == 0). tex == nullptr: solid colour, u/v ignored. */
    virtual void drawTriangles(ITexture* tex, const Vertex* vertices, int vertexCount) = 0;
    /** Colour write mask + blend function. Blending is always on. */
    virtual void setDrawMode(const DrawModeState& mode) = 0;
    /** Scissor in pixels of the current target, y UP from its bottom-left (glScissor convention). */
    virtual void setScissor(bool enabled, const IRect& rect) = 0;
    /** Draws into `target` (nullptr = the window surface); the viewport and projection come from `vm`. */
    virtual void setTarget(IRenderTarget* target, const ViewportMap& vm) = 0;
    /** Clears the colour buffer of the current target. Like glClear it honours the scissor and the colour mask. */
    virtual void clear(const Color& color) = 0;
    /** Submits the pending batch. */
    virtual void flush() = 0;
};

/** Decoded pixels of an image file or APK asset (assets/res/...). */
class IImageDecoder {
public:
    virtual ~IImageDecoder() = default;
    /** false if the file is missing or not decodable. */
    virtual bool decode(const std::string& path, Pixmap& out) = 0;
};

struct GlyphBitmap {
    int width = 0, height = 0;
    int left = 0;  ///< FreeType bitmap_left: pixels from the pen to the bitmap's left edge
    int top = 0;   ///< FreeType bitmap_top: pixels from the baseline up to the bitmap's top row
    std::vector<uint8_t> coverage;  ///< width * height, 8 bit, row 0 = top
};

/** One font file at one pixel size (the role of the FreeType Face + size in DynamicFreeTypeFont). */
class IFontFace {
public:
    virtual ~IFontFace() = default;
    /** Glyph index != 0 for this code point. */
    virtual bool hasGlyph(char32_t cp) const = 0;
    /** Pixels above / below the baseline (descent is NEGATIVE), like FreeType's size metrics. */
    virtual int ascent() const = 0;
    virtual int descent() const = 0;
    /** Horizontal advance in pixels of the glyph for cp (glyph 0, the "missing" glyph, if the font lacks it). */
    virtual float advance(char32_t cp) = 0;
    /** Renders the glyph (coverage may be empty for blanks); false on failure. */
    virtual bool rasterize(char32_t cp, GlyphBitmap& out) = 0;
};

/**
 * Everything the pure code needs from the platform: texture and target creation, image decoding, font files.
 * One per app, installed with setRenderDevice().
 */
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    virtual IRenderBackend& backend() = 0;
    /** Called after a lost graphics context was recreated, once per live texture: refill it with update(). */
    using TextureRestorer = std::function<void(ITexture&)>;
    /** rgba may be null (contents undefined). `restore` is kept by the device and called after context loss. */
    virtual std::shared_ptr<ITexture> createTexture(int w, int h, const uint8_t* rgba, TextureFilter filter,
                                                    TextureRestorer restore) = 0;
    virtual std::shared_ptr<IRenderTarget> createRenderTarget(int w, int h) = 0;
    virtual IImageDecoder& imageDecoder() = 0;
    /** `name` as the game knows it (e.g. "Exo2-Regular.ttf"); the device looks in the file system, then the assets. null if missing. */
    virtual std::shared_ptr<IFontFace> loadFontFace(const std::string& name, float pixelSize) = 0;
    /** Largest texture side the GPU accepts. */
    virtual int maxTextureSize() const { return 2048; }
    /** Screen size in pixels (used for the image size cap, like Image.getTexCap). */
    virtual void screenSize(int& w, int& h) const = 0;
};

/** Registry, like setAudioBackend(). The device must outlive everything that draws. */
void setRenderDevice(std::shared_ptr<IRenderDevice> device);
IRenderDevice* renderDevice();  ///< nullptr if none installed

}  // namespace opsu
