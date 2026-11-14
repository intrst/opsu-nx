#pragma once
#include <cmath>
#include <string>
#include "opsu/util/JavaCompat.h"

namespace opsu {

/** 2D vector. Port of itdelatrisu.opsu.objects.curves.Vec2f. */
struct Vec2f {
    float x = 0.f, y = 0.f;

    Vec2f() = default;
    Vec2f(float nx, float ny) : x(nx), y(ny) {}

    Vec2f& set(float nx, float ny) { x = nx; y = ny; return *this; }
    Vec2f midPoint(const Vec2f& o) const { return Vec2f((x + o.x) / 2, (y + o.y) / 2); }
    Vec2f& scale(float s) { x *= s; y *= s; return *this; }
    Vec2f& add(const Vec2f& o) { x += o.x; y += o.y; return *this; }
    Vec2f& add(float nx, float ny) { x += nx; y += ny; return *this; }
    Vec2f& sub(const Vec2f& o) { x -= o.x; y -= o.y; return *this; }

    /** Sets this vector to its normal (perpendicular). */
    Vec2f& nor() { float nx = -y, ny = x; x = nx; y = ny; return *this; }

    /** Turns this vector into a unit vector. */
    Vec2f& normalize() { float l = len(); x /= l; y /= l; return *this; }

    Vec2f cpy() const { return Vec2f(x, y); }
    float len() const { return std::sqrt(x * x + y * y); }

    bool operator==(const Vec2f& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2f& o) const { return !(*this == o); }

    /** Java: String.format("(%.3f, %.3f)") -- HALF_UP rounding, unlike printf. */
    std::string toString() const {
        return "(" + java::formatFixed(x, 3) + ", " + java::formatFixed(y, 3) + ")";
    }
};

}  // namespace opsu
