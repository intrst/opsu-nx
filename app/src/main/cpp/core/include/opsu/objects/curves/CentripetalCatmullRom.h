#pragma once
#include <array>
#include <vector>
#include "opsu/objects/curves/CurveType.h"

namespace opsu {

/** Catmull-Rom spline segment through 4 control points. Port of CentripetalCatmullRom.java. */
class CentripetalCatmullRom : public CurveType {
public:
    /** @throws std::invalid_argument unless exactly 4 points are given. */
    explicit CentripetalCatmullRom(const std::vector<Vec2f>& points);
    Vec2f pointAt(float t) const override;

private:
    std::array<float, 4> time_{};
    std::array<Vec2f, 4> points_;
};

}  // namespace opsu
