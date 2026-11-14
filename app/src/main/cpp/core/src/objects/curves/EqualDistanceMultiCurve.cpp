#include "opsu/objects/curves/EqualDistanceMultiCurve.h"
#include <cmath>
#include <stdexcept>
#include "opsu/Utils.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"

namespace opsu {

void EqualDistanceMultiCurve::init(const std::vector<std::unique_ptr<CurveType>>& curvesList) {
    if (curvesList.empty()) throw std::runtime_error("EqualDistanceMultiCurve::init: no curves");
    const HitObject& hitObject = *hitObject_;

    // now try to create points that are equidistant to each other
    ncurve_ = java::toInt(hitObject.getPixelLength() / CURVE_POINTS_SEPERATION);
    // Java: `new Vec2f[ncurve + 1]` -- int arithmetic wraps, a negative length throws
    int curveLength = static_cast<int>(static_cast<unsigned>(ncurve_) + 1u);
    if (curveLength < 0) throw java::NegativeArraySizeException(std::to_string(curveLength));
    curve_.assign(static_cast<size_t>(curveLength), Vec2f());

    float distanceAt = 0;
    size_t iter = 0;
    int curPoint = 0;
    const CurveType* curCurve = curvesList[iter++].get();
    Vec2f lastCurve = curCurve->getCurvePoint()[0];
    float lastDistanceAt = 0;

    // length of Curve should equal pixel length (in 640x480)
    float pixelLength = hitObject.getPixelLength() * HitObject::getXMultiplier();

    // for each distance, try to get in between the two points that are between it
    for (int i = 0; i < ncurve_ + 1; i++) {
        int prefDistance = java::toInt(static_cast<float>(i) * pixelLength / static_cast<float>(ncurve_));
        while (distanceAt < prefDistance) {
            lastDistanceAt = distanceAt;
            lastCurve = curCurve->getCurvePoint()[curPoint];
            curPoint++;

            if (curPoint >= curCurve->getCurvesCount()) {
                if (iter < curvesList.size()) {
                    curCurve = curvesList[iter++].get();
                    curPoint = 0;
                } else {
                    curPoint = curCurve->getCurvesCount() - 1;
                    if (lastDistanceAt == distanceAt) {
                        // out of points even though the preferred distance hasn't been reached
                        break;
                    }
                }
            }
            distanceAt += curCurve->getCurveDistances()[curPoint];
        }
        Vec2f thisCurve = curCurve->getCurvePoint()[curPoint];

        // interpolate the point between the two closest distances
        if (distanceAt - lastDistanceAt > 1) {
            float t = (static_cast<float>(prefDistance) - lastDistanceAt) / (distanceAt - lastDistanceAt);
            curve_[i] = Vec2f(Utils::lerp(lastCurve.x, thisCurve.x, t), Utils::lerp(lastCurve.y, thisCurve.y, t));
        } else
            curve_[i] = thisCurve;
    }

    Vec2f c1 = curve_.at(0);  // Java: ArrayIndexOutOfBoundsException when the array is empty (ncurve == -1)
    int cnt = 1;
    if (cnt > ncurve_) return;

    Vec2f c2 = curve_[cnt++];
    while (cnt <= ncurve_ && c2.cpy().sub(c1).len() < 1) c2 = curve_[cnt++];
    startAngle_ = static_cast<float>(
        std::atan2(static_cast<double>(c2.y - c1.y), static_cast<double>(c2.x - c1.x)) * 180 / M_PI);

    c1 = curve_[ncurve_];
    cnt = ncurve_ - 1;
    c2 = curve_[cnt--];
    while (cnt >= 0 && c2.cpy().sub(c1).len() < 1) c2 = curve_[cnt--];
    endAngle_ = static_cast<float>(
        std::atan2(static_cast<double>(c2.y - c1.y), static_cast<double>(c2.x - c1.x)) * 180 / M_PI);
}

Vec2f EqualDistanceMultiCurve::pointAt(float t) const {
    float indexF = t * static_cast<float>(ncurve_);
    int index = java::toInt(indexF);
    if (index >= ncurve_) return curve_.at(static_cast<size_t>(ncurve_)).cpy();
    const Vec2f& poi = curve_.at(static_cast<size_t>(index));
    const Vec2f& poi2 = curve_.at(static_cast<size_t>(index) + 1);
    float t2 = indexF - static_cast<float>(index);
    return Vec2f(Utils::lerp(poi.x, poi2.x, t2), Utils::lerp(poi.y, poi2.y, t2));
}

}  // namespace opsu
