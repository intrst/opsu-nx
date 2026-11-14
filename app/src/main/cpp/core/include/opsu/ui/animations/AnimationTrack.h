#pragma once
#include <vector>
#include "opsu/ui/animations/AnimationEquation.h"

namespace opsu {

/**
 * A keyframe track for a single float property, evaluated as a pure function of time.
 * Port of AnimationTrack.java. Builder methods return *this so they chain like the Java ones:
 *   AnimationTrack(0.6f).to(1.1f, 96, AnimationEquation::LINEAR).delay(24)
 */
class AnimationTrack {
public:
    /** @param initial the value before the first segment starts (at time 0) */
    explicit AnimationTrack(float initial);

    /** Tweens from the current value to target over ms, starting when the previous segment ends. */
    AnimationTrack& to(float target, float ms, AnimationEquation eq);
    /** Holds the current value for ms. */
    AnimationTrack& delay(float ms);
    /** Jumps to value instantly (zero-length segment). */
    AnimationTrack& set(float value);

    /** Total length of the track, in milliseconds. */
    float getDuration() const { return cursorTime; }
    /** Final value of the track. */
    float getFinalValue() const { return cursorValue; }
    /** Value at time t (ms since the track started); initial value before the start, final after the end. */
    float valueAt(float t) const;

    /** Eased progress [0,1] of a single transition that starts at start and lasts ms. */
    static float progress(float t, float start, float ms, AnimationEquation eq);

private:
    void add(float b, float d, float f, float t, AnimationEquation eq);

    float initial;
    std::vector<float> begin, duration, from, toV;
    std::vector<AnimationEquation> easing;
    float cursorTime = 0.f;
    float cursorValue;
};

}  // namespace opsu
