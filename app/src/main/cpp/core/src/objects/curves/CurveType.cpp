#include "opsu/objects/curves/CurveType.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

void CurveType::init(float approxlength) {
    // subdivide the curve
    ncurve_ = java::toInt(approxlength / 4) + 2;  // Java (int) cast semantics (NaN -> 0, saturating)
    curve_.assign(ncurve_, Vec2f());
    for (int i = 0; i < ncurve_; i++)
        curve_[i] = pointAt(i / static_cast<float>(ncurve_ - 1));

    // distance of each point from the previous point
    curveDis_.assign(ncurve_, 0.f);
    totalDistance_ = 0.f;
    for (int i = 0; i < ncurve_; i++) {
        curveDis_[i] = (i == 0) ? 0.f : curve_[i].cpy().sub(curve_[i - 1]).len();
        totalDistance_ += curveDis_[i];
    }
}

}  // namespace opsu
