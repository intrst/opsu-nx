#include "opsu/objects/curves/CircumscribedCircle.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "opsu/Utils.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"

namespace opsu {

namespace {
const float TWO_PI = static_cast<float>(M_PI * 2);
const float HALF_PI = static_cast<float>(M_PI / 2);
}  // namespace

CircumscribedCircle::CircumscribedCircle(const HitObject& hitObject, bool scaled) : Curve(hitObject, scaled) {
    // construct the three points
    Vec2f start(getX(0), getY(0));
    Vec2f mid(getX(1), getY(1));
    Vec2f end(getX(2), getY(2));

    // find the circle center
    Vec2f mida = start.midPoint(mid);
    Vec2f midb = end.midPoint(mid);
    Vec2f nora = mid.cpy().sub(start).nor();
    Vec2f norb = mid.cpy().sub(end).nor();

    circleCenter_ = intersect(mida, nora, midb, norb);

    // find the angles relative to the circle center
    Vec2f startAngPoint = start.cpy().sub(circleCenter_);
    Vec2f midAngPoint = mid.cpy().sub(circleCenter_);
    Vec2f endAngPoint = end.cpy().sub(circleCenter_);

    // (Java: Math.atan2 on floats widened to double)
    startAng_ = static_cast<float>(std::atan2(static_cast<double>(startAngPoint.y), static_cast<double>(startAngPoint.x)));
    midAng_ = static_cast<float>(std::atan2(static_cast<double>(midAngPoint.y), static_cast<double>(midAngPoint.x)));
    endAng_ = static_cast<float>(std::atan2(static_cast<double>(endAngPoint.y), static_cast<double>(endAngPoint.x)));

    // find the angles that pass through midAng
    if (!isIn(startAng_, midAng_, endAng_)) {
        if (std::fabs(startAng_ + TWO_PI - endAng_) < TWO_PI && isIn(startAng_ + TWO_PI, midAng_, endAng_))
            startAng_ += TWO_PI;
        else if (std::fabs(startAng_ - (endAng_ + TWO_PI)) < TWO_PI && isIn(startAng_, midAng_, endAng_ + TWO_PI))
            endAng_ += TWO_PI;
        else if (std::fabs(startAng_ - TWO_PI - endAng_) < TWO_PI && isIn(startAng_ - TWO_PI, midAng_, endAng_))
            startAng_ -= TWO_PI;
        else if (std::fabs(startAng_ - (endAng_ - TWO_PI)) < TWO_PI && isIn(startAng_, midAng_, endAng_ - TWO_PI))
            endAng_ -= TWO_PI;
        else
            throw std::runtime_error("Cannot find angles between midAng (" + java::formatFixed(startAng_, 3) + " " +
                                     java::formatFixed(midAng_, 3) + " " + java::formatFixed(endAng_, 3) + ").");
    }

    // find an angle with an arc length of pixelLength along this circle
    radius_ = startAngPoint.len();
    float pixelLength = hitObject.getPixelLength() * HitObject::getXMultiplier();
    float arcAng = pixelLength / radius_;  // len = theta * r / theta = len / r

    // now use it for our new end angle
    endAng_ = (endAng_ > startAng_) ? startAng_ + arcAng : startAng_ - arcAng;

    // finds the angles to draw for repeats
    drawEndAngle_ = static_cast<float>((endAng_ + (startAng_ > endAng_ ? HALF_PI : -HALF_PI)) * 180 / M_PI);
    drawStartAngle_ = static_cast<float>((startAng_ + (startAng_ > endAng_ ? -HALF_PI : HALF_PI)) * 180 / M_PI);

    // calculate points
    float step = hitObject.getPixelLength() / CURVE_POINTS_SEPERATION;
    // Java: `new Vec2f[(int) step + 1]` -- int arithmetic wraps, a negative length throws
    int curveLength = static_cast<int>(static_cast<unsigned>(java::toInt(step)) + 1u);
    if (curveLength < 0) throw java::NegativeArraySizeException(std::to_string(curveLength));
    curve_.assign(static_cast<size_t>(curveLength), Vec2f());
    for (size_t i = 0; i < curve_.size(); i++) curve_[i] = pointAt(static_cast<float>(i) / step);
}

float CircumscribedCircle::indexAt(float t) const {
    // point i lies at t = i / step, with a (non-integer) step of pixelLength / CURVE_POINTS_SEPERATION
    float index = std::max(0.f, std::min(t, 1.f)) * (hitObject_->getPixelLength() / CURVE_POINTS_SEPERATION);
    return std::min(index, static_cast<float>(curve_.size()) - 1.f);
}

Vec2f CircumscribedCircle::intersect(const Vec2f& a, const Vec2f& ta, const Vec2f& b, const Vec2f& tb) {
    // xy = a + ta * t = b + tb * u  (see the derivation in the Java original)
    float des = tb.x * ta.y - tb.y * ta.x;
    if (std::fabs(des) < 0.00001f) throw std::runtime_error("Vectors are parallel.");
    float u = ((b.y - a.y) * ta.x + (a.x - b.x) * ta.y) / des;
    return b.cpy().add(tb.x * u, tb.y * u);
}

Vec2f CircumscribedCircle::pointAt(float t) const {
    float ang = Utils::lerp(startAng_, endAng_, t);
    return Vec2f(static_cast<float>(std::cos(static_cast<double>(ang)) * radius_ + circleCenter_.x),
                 static_cast<float>(std::sin(static_cast<double>(ang)) * radius_ + circleCenter_.y));
}

}  // namespace opsu
