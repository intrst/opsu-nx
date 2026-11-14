#pragma once
#include "opsu/objects/curves/EqualDistanceMultiCurve.h"

namespace opsu {

/** Bezier curve with equidistant points. Port of LinearBezier.java. */
class LinearBezier : public EqualDistanceMultiCurve {
public:
    /** @param line whether a new curve should be generated for each sequential pair */
    LinearBezier(const HitObject& hitObject, bool line, bool scaled = true);
};

}  // namespace opsu
