#include "opsu/gfx/ViewportMap.h"

#include <algorithm>
#include <cmath>

#include "opsu/util/JavaCompat.h"

namespace opsu {

IRect ViewportMap::viewport() const {
    if (mode == Mode::Stretch || virtualW <= 0.f || virtualH <= 0.f) return IRect{0, 0, pixelW, pixelH};
    const float s = std::min(pixelW / virtualW, pixelH / virtualH);
    const int w = std::max(1, static_cast<int>(std::lround(virtualW * s)));
    const int h = std::max(1, static_cast<int>(std::lround(virtualH * s)));
    return IRect{(pixelW - w) / 2, (pixelH - h) / 2, w, h};
}

float ViewportMap::scaleX() const { return virtualW > 0.f ? viewport().w / virtualW : 1.f; }
float ViewportMap::scaleY() const { return virtualH > 0.f ? viewport().h / virtualH : 1.f; }

void ViewportMap::toGlPixels(float x, float y, float& px, float& py) const {
    const IRect v = viewport();
    px = v.x + x / virtualW * v.w;
    py = v.y + (1.f - y / virtualH) * v.h;
}

bool computeScissor(const Transform2D& t, const ViewportMap& vm, int x, int y, int w, int h, IRect& out) {
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;
    float tx0, ty0, tx1, ty1;
    t.apply(static_cast<float>(x), static_cast<float>(y), tx0, ty0);
    t.apply(static_cast<float>(x) + static_cast<float>(w), static_cast<float>(y) + static_cast<float>(h), tx1, ty1);
    float px0, py0, px1, py1;
    vm.toGlPixels(tx0, ty0, px0, py0);
    vm.toGlPixels(tx1, ty1, px1, py1);
    float sx = px0, sy = py0, sw = px1 - px0, sh = py1 - py0;
    if (sw < 0.f) { sw = -sw; sx -= sw; }  // ScissorStack.fix
    if (sh < 0.f) { sh = -sh; sy -= sh; }
    if (!(sw >= 1.f) || !(sh >= 1.f)) return false;  // also catches NaN
    out = IRect{java::toInt(sx), java::toInt(sy), java::toInt(sw), java::toInt(sh)};
    return true;
}

}  // namespace opsu
