#include "opsu/ui/KineticScrolling.h"
#include <cmath>
#include <stdexcept>
#include "opsu/util/JavaMath.h"

namespace opsu {
namespace {
/** The constant used to determine how fast the target position will be reached. */
const int TIME_CONST = 200;
/** The constant used to determine how much of the velocity will be used to launch to the target. */
const float AMPLITUDE_CONST = 0.25f;
}  // namespace

void KineticScrolling::update(float delta) {
    if (!pressedFlag) {
        totalDelta += delta;
        position = target + static_cast<float>(-amplitude * std::exp(static_cast<double>(-totalDelta / (TIME_CONST / speedMultiplier))));
    } else {
        if (delta > 60) delta = 60;
        float delta2 = delta / 60.f;
        float minusdelta2 = 1 - delta2;
        avgVelocity = (minusdelta2 * avgVelocity + delta2 * (deltaPosition * 1000.f / delta));

        position += deltaPosition;
        target = position;
        deltaPosition = 0;
    }
    if (allowOverScroll && pressedFlag) return;
    if (position > max) {
        if (allowOverScroll) {
            scrollToPosition(max);
        } else {
            amplitude = 0;
            target = position = max;
        }
    }
    if (position < min) {
        if (allowOverScroll) {
            scrollToPosition(min);
        } else {
            amplitude = 0;
            target = position = min;
        }
    }
}

void KineticScrolling::scrollToPosition(float newPosition) {
    pressed();
    released();
    amplitude = newPosition - position;
    target = newPosition;
    totalDelta = 0;
}

void KineticScrolling::setPosition(float newPosition) {
    pressed();
    released();
    target = newPosition;
    position = target;
}

void KineticScrolling::pressed() {
    if (pressedFlag) return;
    pressedFlag = true;
    avgVelocity = 0;
    deltaPosition = 0;
}

void KineticScrolling::released() {
    if (!pressedFlag) return;
    pressedFlag = false;
    amplitude = AMPLITUDE_CONST * avgVelocity;
    target = static_cast<float>(java::round(target + amplitude));
    totalDelta = 0;
}

void KineticScrolling::setSpeedMultiplier(float multiplier) {
    if (multiplier <= 0.f) throw std::invalid_argument("Speed multiplier must be above zero.");
    speedMultiplier = multiplier;
}

}  // namespace opsu
