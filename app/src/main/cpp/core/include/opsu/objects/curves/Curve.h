#pragma once
#include <vector>
#include "opsu/objects/curves/Vec2f.h"

namespace opsu {

class HitObject;

/**
 * Representation of a curve. Port of itdelatrisu.opsu.objects.curves.Curve (geometry half).
 *
 * NOT ported yet (needs SliderBodyRenderer/Color/Options): static init(width,height,...),
 * borderColor, renderer, draw(), drawRange(), splice(), discardGeometry().
 */
class Curve {
public:
    virtual ~Curve() = default;

    /** Points generated along the curve are spaced this far apart. */
    static constexpr float CURVE_POINTS_SEPERATION = 5;

    const std::vector<Vec2f>& getCurvePoints() const { return curve_; }

    /** Returns the point on the curve at t in [0, 1]. */
    virtual Vec2f pointAt(float t) const = 0;

    /** Converts a progress along the curve to a (fractional) index into the curve points. */
    virtual float indexAt(float t) const;

    virtual float getEndAngle() const = 0;
    virtual float getStartAngle() const = 0;

    /** Scaled x/y of control point i (throws std::out_of_range like Java's array access). */
    float getX(int i) const { return i == 0 ? x_ : sliderX_.at(static_cast<size_t>(i - 1)); }
    float getY(int i) const { return i == 0 ? y_ : sliderY_.at(static_cast<size_t>(i - 1)); }

protected:
    /** The HitObject must outlive the curve (non-owning). */
    Curve(const HitObject& hitObject, bool scaled);

    const HitObject* hitObject_;
    float x_, y_;
    std::vector<float> sliderX_, sliderY_;
    std::vector<Vec2f> curve_;
};

}  // namespace opsu
