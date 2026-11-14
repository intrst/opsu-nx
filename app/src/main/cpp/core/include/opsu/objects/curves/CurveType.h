#pragma once
#include <vector>
#include "opsu/objects/curves/Vec2f.h"

namespace opsu {

/** Base class for a single curve segment. Port of CurveType.java. */
class CurveType {
public:
    virtual ~CurveType() = default;

    /** Returns the point on the curve at t in [0, 1]. */
    virtual Vec2f pointAt(float t) const = 0;

    const std::vector<Vec2f>& getCurvePoint() const { return curve_; }
    const std::vector<float>& getCurveDistances() const { return curveDis_; }
    int getCurvesCount() const { return ncurve_; }
    float totalDistance() const { return totalDistance_; }

protected:
    /**
     * Initializes the curve points and distances.
     * Must be called by derived classes from their constructor body
     * (the derived pointAt() is already live at that point).
     */
    void init(float approxlength);

private:
    std::vector<Vec2f> curve_;
    std::vector<float> curveDis_;
    int ncurve_ = 0;
    float totalDistance_ = 0.f;
};

}  // namespace opsu
