#include "opsu/ui/animations/AnimationTrack.h"
#include "opsu/util/JavaMath.h"

namespace opsu {

AnimationTrack::AnimationTrack(float initial) : initial(initial), cursorValue(initial) {}

AnimationTrack& AnimationTrack::to(float target, float ms, AnimationEquation eq) {
    add(cursorTime, java::max(0.f, ms), cursorValue, target, eq);
    cursorTime += java::max(0.f, ms);
    cursorValue = target;
    return *this;
}

AnimationTrack& AnimationTrack::delay(float ms) { return to(cursorValue, ms, AnimationEquation::LINEAR); }

AnimationTrack& AnimationTrack::set(float value) {
    add(cursorTime, 0.f, value, value, AnimationEquation::LINEAR);
    cursorValue = value;
    return *this;
}

void AnimationTrack::add(float b, float d, float f, float t, AnimationEquation eq) {
    begin.push_back(b);
    duration.push_back(d);
    from.push_back(f);
    toV.push_back(t);
    easing.push_back(eq);
}

float AnimationTrack::valueAt(float t) const {
    for (int i = static_cast<int>(begin.size()) - 1; i >= 0; i--) {
        if (t >= begin[i]) {
            float d = duration[i];
            if (d <= 0.f || t >= begin[i] + d) return toV[i];
            return from[i] + (toV[i] - from[i]) * calc(easing[i], (t - begin[i]) / d);
        }
    }
    return initial;
}

float AnimationTrack::progress(float t, float start, float ms, AnimationEquation eq) {
    if (ms <= 0.f) return (t >= start) ? 1.f : 0.f;
    float p = (t - start) / ms;
    return calc(eq, (p < 0.f) ? 0.f : (p > 1.f) ? 1.f : p);
}

}  // namespace opsu
