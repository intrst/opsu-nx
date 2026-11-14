#include "opsu/objects/curves/Curve.h"
#include <algorithm>
#include "opsu/beatmap/HitObject.h"

namespace opsu {

Curve::Curve(const HitObject& hitObject, bool scaled) : hitObject_(&hitObject) {
    if (scaled) {
        x_ = hitObject.getScaledX();
        y_ = hitObject.getScaledY();
        sliderX_ = hitObject.getScaledSliderX();
        sliderY_ = hitObject.getScaledSliderY();
    } else {
        x_ = hitObject.getX();
        y_ = hitObject.getY();
        sliderX_ = hitObject.getSliderX();
        sliderY_ = hitObject.getSliderY();
    }
}

float Curve::indexAt(float t) const {
    if (curve_.empty()) return 0.f;
    return std::max(0.f, std::min(t, 1.f)) * static_cast<float>(curve_.size() - 1);
}

}  // namespace opsu
