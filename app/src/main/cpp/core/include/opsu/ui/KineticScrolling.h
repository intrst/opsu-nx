#pragma once
#include <limits>

namespace opsu {

/**
 * Drag to scroll, based on http://ariya.ofilabs.com/2013/11/javascript-kinetic-scrolling-part-2.html
 * Port of KineticScrolling.java (by fluddokt).
 */
class KineticScrolling {
public:
    void setAllowOverScroll(bool allow) { allowOverScroll = allow; }
    float getPosition() const { return position; }
    float getTargetPosition() const { return target; }
    float getMin() const { return min; }
    float getMax() const { return max; }
    bool isPressed() const { return pressedFlag; }

    /** Updates the scrolling by the elapsed time (ms) since the last update. */
    void update(float delta);
    void scrollToPosition(float newPosition);
    void scrollOffset(float offset) { scrollToPosition(target + offset); }
    void setPosition(float newPosition);
    void addOffset(float offset) { setPosition(position + offset); }
    void pressed();
    void released();
    void dragged(float distance) { deltaPosition += distance; }
    void setMinMax(float mn, float mx) { min = mn; max = mx; }
    /** @throws std::invalid_argument if multiplier <= 0 */
    void setSpeedMultiplier(float multiplier);
    float getSpeedMultiplier() const { return speedMultiplier; }

private:
    float position = 0.f;
    float amplitude = 0.f;
    float target = 0.f;
    float totalDelta = 0.f;
    float max = std::numeric_limits<float>::max();
    float min = -std::numeric_limits<float>::max();
    bool pressedFlag = false;
    float deltaPosition = 0.f;
    float avgVelocity = 0.f;
    float speedMultiplier = 1.f;
    bool allowOverScroll = false;
};

}  // namespace opsu
