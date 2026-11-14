#pragma once
#include "opsu/gfx/Transform2D.h"

namespace opsu {

struct IRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const IRect& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

/**
 * Maps the game's virtual coordinates (y down, 0..virtualW x 0..virtualH, AFTER the Graphics transform) to a
 * render target's pixels. The backend uses it for the projection matrix and glViewport, Graphics uses it for
 * scissor rectangles and for line widths in screen pixels. No letterboxing logic is needed in the game.
 *
 * Default (Java behaviour): the virtual size equals the surface size, so the mapping is 1:1.
 * Stretch: the virtual space fills the whole target (aspect ratio may change).
 * Fit:     uniform scale, centred, bars at the sides (the viewport is the letterboxed rectangle).
 */
struct ViewportMap {
    enum class Mode { Stretch, Fit };

    float virtualW = 1.f, virtualH = 1.f;
    int pixelW = 1, pixelH = 1;
    Mode mode = Mode::Stretch;

    ViewportMap() = default;
    ViewportMap(float vw, float vh, int pw, int ph, Mode m = Mode::Stretch)
        : virtualW(vw), virtualH(vh), pixelW(pw), pixelH(ph), mode(m) {}

    /** The area of the target the virtual space is drawn into (GL viewport: x, y from the bottom-left). */
    IRect viewport() const;
    /** Pixels per virtual unit. */
    float scaleX() const;
    float scaleY() const;

    /** Virtual point to window pixels with y UP from the bottom (what glScissor/glViewport use). */
    void toGlPixels(float x, float y, float& px, float& py) const;
};

/**
 * Port of Graphics.setClip's scissor: the two corners (x, y) and (x + w, y + h) go through the transform and the
 * viewport, the box is normalised, and each value is truncated to int (libGDX ScissorStack + glScissor).
 * w/h <= 0 are first raised to 1. Quirk kept from libGDX: a box smaller than 1 pixel in either direction is NOT
 * pushed, so no clipping happens at all; then this returns false.
 * Rotation: like libGDX only the two corners are used (the box is not the exact bounding box of a rotated rect).
 */
bool computeScissor(const Transform2D& t, const ViewportMap& vm, int x, int y, int w, int h, IRect& out);

}  // namespace opsu
