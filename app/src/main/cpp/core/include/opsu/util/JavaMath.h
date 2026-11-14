#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace opsu {
namespace java {

/** Math.min/Math.max(float,float): NaN wins, -0.0f < 0.0f (std::min/max do neither). */
inline float min(float a, float b) {
    if (a != a) return a;
    if (a == 0.f && b == 0.f) return std::signbit(a) ? a : b;
    return a <= b ? a : b;
}
inline float max(float a, float b) {
    if (a != a) return a;
    if (a == 0.f && b == 0.f) return std::signbit(a) ? b : a;
    return a >= b ? a : b;
}
inline double min(double a, double b) {
    if (a != a) return a;
    if (a == 0. && b == 0.) return std::signbit(a) ? a : b;
    return a <= b ? a : b;
}
inline double max(double a, double b) {
    if (a != a) return a;
    if (a == 0. && b == 0.) return std::signbit(a) ? b : a;
    return a >= b ? a : b;
}
inline int min(int a, int b) { return a < b ? a : b; }
inline int max(int a, int b) { return a > b ? a : b; }

/** Math.round(float): floor(x + 0.5) saturated to int, NaN -> 0 (Java never rounds half to even). */
inline int round(float x) {
    if (x != x) return 0;
    double r = std::floor(static_cast<double>(x) + 0.5);
    if (r >= 2147483647.0) return std::numeric_limits<int>::max();
    if (r <= -2147483648.0) return std::numeric_limits<int>::min();
    return static_cast<int>(r);
}
/** Math.round(double) -> long. */
inline int64_t round(double x) {
    if (x != x) return 0;
    double r = std::floor(x + 0.5);
    if (r >= 9223372036854775807.0) return std::numeric_limits<int64_t>::max();
    if (r <= -9223372036854775808.0) return std::numeric_limits<int64_t>::min();
    return static_cast<int64_t>(r);
}

/** Float.floatToIntBits: like the raw bits, but every NaN collapses to the canonical 0x7fc00000. */
inline int32_t floatToIntBits(float f) {
    if (f != f) return 0x7fc00000;
    int32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}
/** Float.compare(a, b): total order, -0.0f < 0.0f and NaN greater than everything (equal to itself). */
inline int compare(float a, float b) {
    if (a < b) return -1;
    if (a > b) return 1;
    int32_t x = floatToIntBits(a), y = floatToIntBits(b);
    return x == y ? 0 : (x < y ? -1 : 1);
}
/** Double.doubleToLongBits: like the raw bits, but every NaN collapses to the canonical 0x7ff8000000000000. */
inline int64_t doubleToLongBits(double d) {
    if (d != d) return 0x7ff8000000000000LL;
    int64_t u;
    std::memcpy(&u, &d, sizeof u);
    return u;
}
/** Double.compare(a, b): total order, -0.0 < 0.0 and NaN greater than everything (equal to itself). */
inline int compare(double a, double b) {
    if (a < b) return -1;
    if (a > b) return 1;
    int64_t x = doubleToLongBits(a), y = doubleToLongBits(b);
    return x == y ? 0 : (x < y ? -1 : 1);
}
/** fake.Integer2.compare(i, j) = `i - j` (NOT -1/0/1; wraps on overflow like Java int arithmetic). */
inline int integer2Compare(int i, int j) {
    return static_cast<int32_t>(static_cast<uint32_t>(i) - static_cast<uint32_t>(j));
}
/** fake.Long2.compare(i, j) = `(int) (i - j)` (64-bit wrap, then keeps the low 32 bits). */
inline int long2Compare(int64_t i, int64_t j) {
    return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(i) - static_cast<uint64_t>(j)));
}
}  // namespace java
}  // namespace opsu
