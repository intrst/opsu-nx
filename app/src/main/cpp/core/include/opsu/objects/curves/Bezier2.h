#pragma once
#include <cstdint>
#include <vector>
#include "opsu/objects/curves/CurveType.h"

namespace opsu {

/** Bezier curve defined by an arbitrary number of control points. Port of Bezier2.java. */
class Bezier2 : public CurveType {
public:
    explicit Bezier2(std::vector<Vec2f> points);
    Vec2f pointAt(float t) const override;

private:
    static int64_t binomialCoefficient(int n, int k);
    static double bernstein(int i, int n, float t);

    std::vector<Vec2f> points_;
};

}  // namespace opsu
