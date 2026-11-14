#include "opsu/objects/curves/Bezier2.h"
#include <algorithm>
#include <cmath>

namespace opsu {

Bezier2::Bezier2(std::vector<Vec2f> points) : points_(std::move(points)) {
    // approximate by summing the distances between all points
    // (which should be the max possible length of the curve)
    float approxlength = 0;
    for (size_t i = 0; i + 1 < points_.size(); i++)
        approxlength += points_[i].cpy().sub(points_[i + 1]).len();
    init(approxlength);
}

Vec2f Bezier2::pointAt(float t) const {
    Vec2f c;
    int n = static_cast<int>(points_.size()) - 1;
    for (int i = 0; i <= n; i++) {
        double b = bernstein(i, n, t);
        // Java's `c.x += points[i].x * b` (float += float*double) adds in double and narrows once.
        c.x = static_cast<float>(c.x + points_[i].x * b);
        c.y = static_cast<float>(c.y + points_[i].y * b);
    }
    return c;
}

int64_t Bezier2::binomialCoefficient(int n, int k) {
    if (k < 0 || k > n) return 0;
    if (k == 0 || k == n) return 1;
    k = std::min(k, n - k);  // take advantage of symmetry
    int64_t c = 1;
    for (int i = 0; i < k; i++)
        c = c * (n - i) / (i + 1);
    return c;
}

double Bezier2::bernstein(int i, int n, float t) {
    return static_cast<double>(binomialCoefficient(n, i)) * std::pow(t, i) * std::pow(1 - t, n - i);
}

}  // namespace opsu
