#include "opsu/objects/curves/CatmullCurve.h"
#include <deque>
#include "opsu/beatmap/HitObject.h"
#include "opsu/objects/curves/CentripetalCatmullRom.h"

namespace opsu {

CatmullCurve::CatmullCurve(const HitObject& hitObject, bool scaled) : EqualDistanceMultiCurve(hitObject, scaled) {
    std::vector<std::unique_ptr<CurveType>> catmulls;
    int ncontrolPoints = static_cast<int>(hitObject.getSliderX().size()) + 1;
    std::deque<Vec2f> points;  // temporary list of points to separate different curves

    auto toVec = [](const std::deque<Vec2f>& d) { return std::vector<Vec2f>(d.begin(), d.end()); };

    // repeat the first and last points as control points, only if the first/last two points are different
    // aabb
    // aabc abcc
    // aabc abcd bcdd
    if (getX(0) != getX(1) || getY(0) != getY(1)) points.push_back(Vec2f(getX(0), getY(0)));
    for (int i = 0; i < ncontrolPoints; i++) {
        points.push_back(Vec2f(getX(i), getY(i)));
        if (points.size() >= 4) {
            catmulls.push_back(std::make_unique<CentripetalCatmullRom>(toVec(points)));
            points.pop_front();
        }
    }
    if (getX(ncontrolPoints - 1) != getX(ncontrolPoints - 2) || getY(ncontrolPoints - 1) != getY(ncontrolPoints - 2))
        points.push_back(Vec2f(getX(ncontrolPoints - 1), getY(ncontrolPoints - 1)));
    if (points.size() >= 4) catmulls.push_back(std::make_unique<CentripetalCatmullRom>(toVec(points)));

    init(catmulls);
}

}  // namespace opsu
