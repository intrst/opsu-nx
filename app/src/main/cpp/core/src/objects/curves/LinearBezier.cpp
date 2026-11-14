#include "opsu/objects/curves/LinearBezier.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/objects/curves/Bezier2.h"

namespace opsu {

LinearBezier::LinearBezier(const HitObject& hitObject, bool line, bool scaled)
    : EqualDistanceMultiCurve(hitObject, scaled) {
    std::vector<std::unique_ptr<CurveType>> beziers;

    // Beziers: splits points into different Beziers if has the same points (red points)
    // a b c - c d - d e f g
    // Lines: generate a new curve for each sequential pair
    // ab  bc  cd  de  ef  fg
    int controlPoints = static_cast<int>(hitObject.getSliderX().size()) + 1;
    std::vector<Vec2f> points;  // temporary list of points to separate different Bezier curves
    bool hasLast = false;
    Vec2f lastPoi;
    for (int i = 0; i < controlPoints; i++) {
        Vec2f tpoi(getX(i), getY(i));
        if (line) {
            if (hasLast) {
                points.push_back(tpoi);
                beziers.push_back(std::make_unique<Bezier2>(points));
                points.clear();
            }
        } else if (hasLast && tpoi == lastPoi) {
            if (points.size() >= 2) beziers.push_back(std::make_unique<Bezier2>(points));
            points.clear();
        }
        points.push_back(tpoi);
        lastPoi = tpoi;
        hasLast = true;
    }
    if (line || points.size() < 2) {
        // trying to continue Bezier with less than 2 points: probably ending on a red point, ignore it
    } else {
        beziers.push_back(std::make_unique<Bezier2>(points));
        points.clear();
    }

    init(beziers);
}

}  // namespace opsu
