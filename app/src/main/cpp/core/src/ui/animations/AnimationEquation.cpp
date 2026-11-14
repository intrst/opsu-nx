#include "opsu/ui/animations/AnimationEquation.h"
#include <cmath>

// NOTE on float fidelity: every expression keeps Java's float/double promotion exactly
// (float*float stays float; anything touching Math.PI/Math.pow/etc. is double and is
// narrowed where Java casts). Build with -ffp-contract=off.

namespace opsu {
namespace {

const double PI = 3.141592653589793;  // Math.PI

/** Overshoot constant for "back" easings. */
const float OVERSHOOT = 1.70158f;

/** Constants for the osu!lazer elastic easings. */
const double ELASTIC_C = 2 * PI / 0.3;
const double ELASTIC_C2 = 0.3 / 4;
const double ELASTIC_OFFSET_HALF = std::pow(2.0, -10.0) * std::sin((0.5 - ELASTIC_C2) * ELASTIC_C);
const double ELASTIC_OFFSET_QUARTER = std::pow(2.0, -10.0) * std::sin((0.25 - ELASTIC_C2) * ELASTIC_C);

inline float fpow(double a, double b) { return static_cast<float>(std::pow(a, b)); }
inline float fsin(double x) { return static_cast<float>(std::sin(x)); }
inline float fsqrt(double x) { return static_cast<float>(std::sqrt(x)); }

float outBounce(float t) {
    if (t < 0.36363636f)
        return 7.5625f * t * t;
    else if (t < 0.72727273f) {
        t = t - 0.54545454f;
        return 7.5625f * t * t + 0.75f;
    } else if (t < 0.90909091f) {
        t = t - 0.81818182f;
        return 7.5625f * t * t + 0.9375f;
    } else {
        t = t - 0.95454546f;
        return 7.5625f * t * t + 0.984375f;
    }
}

}  // namespace

float calc(AnimationEquation eq, float t) {
    switch (eq) {
        case AnimationEquation::LINEAR: return t;

        /* Quadratic */
        case AnimationEquation::IN_QUAD: return t * t;
        case AnimationEquation::OUT_QUAD: return -1 * t * (t - 2);
        case AnimationEquation::IN_OUT_QUAD:
            t = t * 2;
            if (t < 1) return 0.5f * t * t;
            t = t - 1;
            return -0.5f * (t * (t - 2) - 1);

        /* Cubic */
        case AnimationEquation::IN_CUBIC: return t * t * t;
        case AnimationEquation::OUT_CUBIC:
            t = t - 1;
            return t * t * t + 1;
        case AnimationEquation::IN_OUT_CUBIC:
            t = t * 2;
            if (t < 1) return 0.5f * t * t * t;
            t = t - 2;
            return 0.5f * (t * t * t + 2);

        /* Quartic */
        case AnimationEquation::IN_QUART: return t * t * t * t;
        case AnimationEquation::OUT_QUART:
            t = t - 1;
            return -1 * (t * t * t * t - 1);
        case AnimationEquation::IN_OUT_QUART:
            t = t * 2;
            if (t < 1) return 0.5f * t * t * t * t;
            t = t - 2;
            return -0.5f * (t * t * t * t - 2);

        /* Quintic */
        case AnimationEquation::IN_QUINT: return t * t * t * t * t;
        case AnimationEquation::OUT_QUINT:
            t = t - 1;
            return (t * t * t * t * t + 1);
        case AnimationEquation::IN_OUT_QUINT:
            t = t * 2;
            if (t < 1) return 0.5f * t * t * t * t * t;
            t = t - 2;
            return 0.5f * (t * t * t * t * t + 2);

        /* Sine */
        case AnimationEquation::IN_SINE: return -1 * static_cast<float>(std::cos(t * (PI / 2))) + 1;
        case AnimationEquation::OUT_SINE: return static_cast<float>(std::sin(t * (PI / 2)));
        case AnimationEquation::IN_OUT_SINE: return static_cast<float>(std::cos(PI * t) - 1) / -2;

        /* Exponential */
        case AnimationEquation::IN_EXPO: return (t == 0) ? 0 : fpow(2, 10 * (t - 1));
        case AnimationEquation::OUT_EXPO: return (t == 1) ? 1 : static_cast<float>(-std::pow(2.0, -10 * t)) + 1;
        case AnimationEquation::IN_OUT_EXPO:
            if (t == 0 || t == 1) return t;
            t = t * 2;
            if (t < 1) return 0.5f * fpow(2, 10 * (t - 1));
            t = t - 1;
            return 0.5f * (static_cast<float>(-std::pow(2.0, -10 * t)) + 2);

        /* Circular */
        case AnimationEquation::IN_CIRC: return -1 * (fsqrt(1 - t * t) - 1);
        case AnimationEquation::OUT_CIRC:
            t = t - 1;
            return fsqrt(1 - t * t);
        case AnimationEquation::IN_OUT_CIRC:
            t = t * 2;
            if (t < 1) return -0.5f * (fsqrt(1 - t * t) - 1);
            t = t - 2;
            return 0.5f * (fsqrt(1 - t * t) + 1);

        /* Back */
        case AnimationEquation::IN_BACK: return t * t * ((OVERSHOOT + 1) * t - OVERSHOOT);
        case AnimationEquation::OUT_BACK:
            t = t - 1;
            return t * t * ((OVERSHOOT + 1) * t + OVERSHOOT) + 1;
        case AnimationEquation::IN_OUT_BACK: {
            float overshoot = OVERSHOOT * 1.525f;
            t = t * 2;
            if (t < 1) return 0.5f * (t * t * ((overshoot + 1) * t - overshoot));
            t = t - 2;
            return 0.5f * (t * t * ((overshoot + 1) * t + overshoot) + 2);
        }

        /* Bounce */
        case AnimationEquation::IN_BOUNCE: return 1 - outBounce(1 - t);
        case AnimationEquation::OUT_BOUNCE: return outBounce(t);
        case AnimationEquation::IN_OUT_BOUNCE:
            if (t < 0.5f) return (1 - outBounce(1 - t * 2)) * 0.5f;
            return outBounce(t * 2 - 1) * 0.5f + 0.5f;

        /* Elastic */
        case AnimationEquation::IN_ELASTIC: {
            if (t == 0 || t == 1) return t;
            float period = 0.3f;
            t = t - 1;
            return -(fpow(2, 10 * t) * fsin(((t - period / 4) * (PI * 2)) / period));
        }
        case AnimationEquation::OUT_ELASTIC: {
            if (t == 0 || t == 1) return t;
            float period = 0.3f;
            return fpow(2, -10 * t) * fsin((t - period / 4) * (PI * 2) / period) + 1;
        }
        case AnimationEquation::OUT_ELASTIC_HALF:
            return static_cast<float>(std::pow(2.0, -10 * t) * std::sin((0.5 * t - ELASTIC_C2) * ELASTIC_C) + 1 -
                                      ELASTIC_OFFSET_HALF * t);
        case AnimationEquation::OUT_ELASTIC_QUARTER:
            return static_cast<float>(std::pow(2.0, -10 * t) * std::sin((0.25 * t - ELASTIC_C2) * ELASTIC_C) + 1 -
                                      ELASTIC_OFFSET_QUARTER * t);
        case AnimationEquation::IN_OUT_ELASTIC: {
            if (t == 0 || t == 1) return t;
            float period = 0.44999996f;
            t = t * 2 - 1;
            if (t < 0) return -0.5f * (fpow(2, 10 * t) * fsin((t - period / 4) * (PI * 2) / period));
            return fpow(2, -10 * t) * fsin((t - period / 4) * (PI * 2) / period) * 0.5f + 1;
        }
    }
    return t;
}

}  // namespace opsu
