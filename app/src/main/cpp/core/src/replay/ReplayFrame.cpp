#include "opsu/replay/ReplayFrame.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

int ReplayFrame::getScaledX() const {
    return java::toInt(x_ * HitObject::getXMultiplier() + HitObject::getXOffset());
}
int ReplayFrame::getScaledY() const {
    return java::toInt(y_ * HitObject::getYMultiplier() + HitObject::getYOffset());
}
std::string ReplayFrame::toString() const {
    return "(" + std::to_string(time_) + ", [" + java::formatFixed(x_, 2) + ", " + java::formatFixed(y_, 2) +
           "], " + std::to_string(keys_) + ")";
}

}  // namespace opsu
