#include "opsu/gfx/Transform2D.h"

#include <cmath>
#include <stdexcept>

namespace opsu {

namespace {
void sinCosDeg(float deg, float& s, float& c) {
    // exact for quarter turns
    float r = std::fmod(deg, 360.f);
    if (r < 0.f) r += 360.f;
    if (r == 0.f) { s = 0.f; c = 1.f; return; }
    if (r == 90.f) { s = 1.f; c = 0.f; return; }
    if (r == 180.f) { s = 0.f; c = -1.f; return; }
    if (r == 270.f) { s = -1.f; c = 0.f; return; }
    const float rad = deg * (3.14159265358979323846f / 180.f);
    s = std::sin(rad);
    c = std::cos(rad);
}
}  // namespace

Transform2D& Transform2D::translate(float x, float y) {
    tx = a * x + c * y + tx;
    ty = b * x + d * y + ty;
    return *this;
}

Transform2D& Transform2D::scale(float sx, float sy) {
    a *= sx; b *= sx;
    c *= sy; d *= sy;
    return *this;
}

Transform2D& Transform2D::rotate(float degrees) {
    if (degrees == 0.f) return *this;
    float s, co;
    sinCosDeg(degrees, s, co);
    const float na = a * co + c * s, nb = b * co + d * s;
    const float nc = -a * s + c * co, nd = -b * s + d * co;
    a = na; b = nb; c = nc; d = nd;
    return *this;
}

Transform2D& Transform2D::rotateAbout(float x, float y, float degrees) {
    translate(x, y);
    rotate(degrees);
    translate(-x, -y);
    return *this;
}

Transform2D& Transform2D::mul(const Transform2D& o) {
    const float na = a * o.a + c * o.b, nb = b * o.a + d * o.b;
    const float nc = a * o.c + c * o.d, nd = b * o.c + d * o.d;
    const float ntx = a * o.tx + c * o.ty + tx, nty = b * o.tx + d * o.ty + ty;
    a = na; b = nb; c = nc; d = nd; tx = ntx; ty = nty;
    return *this;
}

bool Transform2D::isScaled() const { return std::fabs(a - 1.f) > 0.001f || std::fabs(d - 1.f) > 0.001f; }

bool Transform2D::invert(Transform2D& out) const {
    const float det = a * d - b * c;
    if (det == 0.f || !std::isfinite(det)) return false;
    const float id = 1.f / det;
    out.a = d * id;
    out.b = -b * id;
    out.c = -c * id;
    out.d = a * id;
    out.tx = -(out.a * tx + out.c * ty);
    out.ty = -(out.b * tx + out.d * ty);
    return true;
}

Transform2D TransformStack::pop() {
    if (stack_.empty()) throw std::out_of_range("TransformStack: pop on empty stack");
    Transform2D t = stack_.back();
    stack_.pop_back();
    return t;
}

}  // namespace opsu
