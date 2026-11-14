#pragma once
#include "opsu/objects/curves/Curve.h"

namespace opsu {

/** Curve along the circumscribed circle of three points. Port of CircumscribedCircle.java. */
class CircumscribedCircle : public Curve {
public:
    /** @throws std::runtime_error if the three points are collinear / no angle fits. */
    explicit CircumscribedCircle(const HitObject& hitObject, bool scaled = true);

    Vec2f pointAt(float t) const override;
    float indexAt(float t) const override;
    float getEndAngle() const override { return drawEndAngle_; }
    float getStartAngle() const override { return drawStartAngle_; }

private:
    static bool isIn(float a, float b, float c) { return (b > a && b < c) || (b < a && b > c); }
    static Vec2f intersect(const Vec2f& a, const Vec2f& ta, const Vec2f& b, const Vec2f& tb);

    Vec2f circleCenter_;
    float radius_ = 0;
    float startAng_ = 0, endAng_ = 0, midAng_ = 0;
    float drawStartAngle_ = 0, drawEndAngle_ = 0;
};

}  // namespace opsu
