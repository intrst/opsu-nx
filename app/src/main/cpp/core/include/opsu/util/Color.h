#pragma once
#include "opsu/util/JavaCompat.h"

namespace opsu {

/**
 * Minimal RGBA colour with the semantics of fluddokt.opsu.fake.Color (which extends libGDX's Color),
 * so beatmap/skin code ports 1:1 and a graphics backend can convert at the edge.
 *
 * Java quirk kept on purpose: getRed()/getGreen()/getBlue()/getAlpha() TRUNCATE ((int)(r * 255)) instead
 * of rounding. For channels built from ints 0..255 this round-trips exactly (checked exhaustively by the
 * golden tests), but colours made from arbitrary floats read back truncated, as in the Java.
 */
struct Color {
    float r = 0.f, g = 0.f, b = 0.f, a = 0.f;

    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1.f) : r(r_), g(g_), b(b_), a(a_) {}
    Color(int r_, int g_, int b_, int a_) : r(r_ / 255.f), g(g_ / 255.f), b(b_ / 255.f), a(a_ / 255.f) {}
    Color(int r_, int g_, int b_) : Color(r_, g_, b_, 1.f) {}
    /** Color(int r, int g, int b, float a): alpha is already a fraction. */
    Color(int r_, int g_, int b_, float a_) : r(r_ / 255.f), g(g_ / 255.f), b(b_ / 255.f), a(a_) {}

    int getRed() const { return java::toInt(r * 255); }
    int getGreen() const { return java::toInt(g * 255); }
    int getBlue() const { return java::toInt(b * 255); }
    int getAlpha() const { return java::toInt(a * 255); }

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

}  // namespace opsu
