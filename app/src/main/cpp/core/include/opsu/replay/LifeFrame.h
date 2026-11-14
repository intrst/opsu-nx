#pragma once
#include <string>
#include "opsu/util/JavaCompat.h"

namespace opsu {

/**
 * Captures a single life frame.
 * Port of itdelatrisu.opsu.replay.LifeFrame (original by smoogipooo).
 */
class LifeFrame {
public:
    /** The sample interval, in milliseconds, when saving replays. */
    static constexpr int SAMPLE_INTERVAL = 2000;

    /** @param time the time (in ms)  @param health the health [0,1] */
    LifeFrame(int time, float health) : time_(time), health_(health) {}

    int getTime() const { return time_; }
    float getHealth() const { return health_; }

    /** Java: String.format("(%d, %.2f)") -- HALF_UP rounding, unlike printf. */
    std::string toString() const {
        return "(" + std::to_string(time_) + ", " + java::formatFixed(health_, 2) + ")";
    }

private:
    int time_;
    float health_;
};

}  // namespace opsu
