#pragma once
#include "opsu/ui/animations/AnimationEquation.h"

namespace opsu {

/** Utility class for updating a value using an animation equation. Port of AnimatedValue.java. */
class AnimatedValue {
public:
    /**
     * @param duration the total animation duration, in milliseconds
     * @param min the minimum value
     * @param max the maximum value
     */
    AnimatedValue(int duration, float min, float max, AnimationEquation eqn);

    float getValue() const { return value; }
    int getTime() const { return time; }
    /** Sets the animation time manually (clamped to [0, duration]). */
    void setTime(int time);
    int getDuration() const { return duration; }
    void setDuration(int duration);
    AnimationEquation getEquation() const { return eqn; }
    void setEquation(AnimationEquation eqn);
    /** Advances by delta ms. Returns true if an update was applied. */
    bool update(int delta);
    /** True if getTime() equals getDuration(). */
    bool isFinished() const { return time == duration; }

private:
    void updateValue();

    int duration;
    int time;
    float base;
    float diff;
    float value;
    AnimationEquation eqn;
};

}  // namespace opsu
