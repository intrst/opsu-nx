#include "opsu/gfx/Texture.h"

#include <utility>

namespace opsu {

void TextureRegion::uvs(float& u0, float& v0, float& u1, float& v1) const {
    if (!tex || tex->width() <= 0 || tex->height() <= 0) { u0 = v0 = u1 = v1 = 0.f; return; }
    const float tw = static_cast<float>(tex->width()), th = static_cast<float>(tex->height());
    u0 = static_cast<float>(x) / tw;
    u1 = static_cast<float>(x + w) / tw;
    v0 = static_cast<float>(y) / th;
    v1 = static_cast<float>(y + h) / th;
    if (tex->rowZeroIsBottom()) { v0 = 1.f - v0; v1 = 1.f - v1; }
    if (flipX) std::swap(u0, u1);
    if (flipY) std::swap(v0, v1);
}

TextureRegion TextureRegion::sub(int rx, int ry, int rw, int rh) const {
    TextureRegion r;
    r.tex = tex;
    r.x = x + rx;
    r.y = y + ry;
    r.w = rw;
    r.h = rh;
    return r;
}

}  // namespace opsu
