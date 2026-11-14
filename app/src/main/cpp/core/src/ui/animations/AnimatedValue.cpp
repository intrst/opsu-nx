#include "opsu/ui/animations/AnimatedValue.h"
#include "opsu/Utils.h"

namespace opsu {

AnimatedValue::AnimatedValue(int duration, float min, float max, AnimationEquation eqn)
    : duration(duration), time(0), base(min), diff(max - min), value(min), eqn(eqn) {}

void AnimatedValue::setTime(int t) {
    time = Utils::clamp(t, 0, duration);
    updateValue();
}

void AnimatedValue::setDuration(int d) {
    duration = d;
    int newTime = Utils::clamp(time, 0, duration);
    if (time != newTime) {
        time = newTime;
        updateValue();
    }
}

void AnimatedValue::setEquation(AnimationEquation e) {
    eqn = e;
    updateValue();
}

bool AnimatedValue::update(int delta) {
    int newTime = Utils::clamp(time + delta, 0, duration);
    if (time != newTime) {
        time = newTime;
        updateValue();
        return true;
    }
    return false;
}

void AnimatedValue::updateValue() {
    // Java: (float) time / duration  -> float division (0/0 gives NaN as in Java)
    float t = calc(eqn, static_cast<float>(time) / static_cast<float>(duration));
    value = base + (t * diff);
}

}  // namespace opsu
