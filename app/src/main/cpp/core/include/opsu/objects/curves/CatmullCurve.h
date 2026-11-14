#pragma once
#include "opsu/objects/curves/EqualDistanceMultiCurve.h"

namespace opsu {

/** Catmull curve with equidistant points. Port of CatmullCurve.java. */
class CatmullCurve : public EqualDistanceMultiCurve {
public:
    explicit CatmullCurve(const HitObject& hitObject, bool scaled = true);
};

}  // namespace opsu
