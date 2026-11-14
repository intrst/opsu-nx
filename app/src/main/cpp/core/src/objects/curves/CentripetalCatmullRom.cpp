#include "opsu/objects/curves/CentripetalCatmullRom.h"
#include <stdexcept>
#include <string>

namespace opsu {

CentripetalCatmullRom::CentripetalCatmullRom(const std::vector<Vec2f>& points) {
    if (points.size() != 4)
        throw std::invalid_argument("Need exactly 4 points to initialize CentripetalCatmullRom, " +
                                    std::to_string(points.size()) + " provided.");
    for (int i = 0; i < 4; i++) points_[i] = points[i];

    time_[0] = 0;
    float approxLength = 0;
    for (int i = 1; i < 4; i++) {
        float len = points_[i].cpy().sub(points_[i - 1]).len();
        if (len <= 0) len += 0.0001f;
        approxLength += len;
        time_[i] = static_cast<float>(i);  // uniform parameterization (as in the Java original)
    }
    init(approxLength / 2);
}

Vec2f CentripetalCatmullRom::pointAt(float t) const {
    const auto& p = points_;
    const auto& tm = time_;
    t = t * (tm[2] - tm[1]) + tm[1];

    Vec2f A1 = p[0].cpy().scale((tm[1] - t) / (tm[1] - tm[0])).add(p[1].cpy().scale((t - tm[0]) / (tm[1] - tm[0])));
    Vec2f A2 = p[1].cpy().scale((tm[2] - t) / (tm[2] - tm[1])).add(p[2].cpy().scale((t - tm[1]) / (tm[2] - tm[1])));
    Vec2f A3 = p[2].cpy().scale((tm[3] - t) / (tm[3] - tm[2])).add(p[3].cpy().scale((t - tm[2]) / (tm[3] - tm[2])));

    Vec2f B1 = A1.cpy().scale((tm[2] - t) / (tm[2] - tm[0])).add(A2.cpy().scale((t - tm[0]) / (tm[2] - tm[0])));
    Vec2f B2 = A2.cpy().scale((tm[3] - t) / (tm[3] - tm[1])).add(A3.cpy().scale((t - tm[1]) / (tm[3] - tm[1])));

    return B1.cpy().scale((tm[2] - t) / (tm[2] - tm[1])).add(B2.cpy().scale((t - tm[1]) / (tm[2] - tm[1])));
}

}  // namespace opsu
