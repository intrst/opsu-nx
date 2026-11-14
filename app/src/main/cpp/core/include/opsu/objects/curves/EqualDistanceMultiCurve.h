#pragma once
#include <memory>
#include <vector>
#include "opsu/objects/curves/Curve.h"
#include "opsu/objects/curves/CurveType.h"

namespace opsu {

/** Multiple curves joined into equidistant points. Port of EqualDistanceMultiCurve.java. */
class EqualDistanceMultiCurve : public Curve {
public:
    Vec2f pointAt(float t) const override;
    float getEndAngle() const override { return endAngle_; }
    float getStartAngle() const override { return startAngle_; }

protected:
    EqualDistanceMultiCurve(const HitObject& hitObject, bool scaled = true) : Curve(hitObject, scaled) {}

    /** Must be called by derived classes. @throws std::runtime_error if the list is empty. */
    void init(const std::vector<std::unique_ptr<CurveType>>& curvesList);

private:
    float startAngle_ = 0, endAngle_ = 0;
    int ncurve_ = 0;
};

}  // namespace opsu
