#pragma once

namespace opsu {

/**
 * Easing functions for animations. Port of itdelatrisu.opsu.ui.animations.AnimationEquation
 * (Robert Penner's equations, BSD licence; see the Java header for the full notice).
 * The Java enum's abstract `calc(t)` is the free function calc(eq, t).
 */
enum class AnimationEquation {
    LINEAR,
    IN_QUAD, OUT_QUAD, IN_OUT_QUAD,
    IN_CUBIC, OUT_CUBIC, IN_OUT_CUBIC,
    IN_QUART, OUT_QUART, IN_OUT_QUART,
    IN_QUINT, OUT_QUINT, IN_OUT_QUINT,
    IN_SINE, OUT_SINE, IN_OUT_SINE,
    IN_EXPO, OUT_EXPO, IN_OUT_EXPO,
    IN_CIRC, OUT_CIRC, IN_OUT_CIRC,
    IN_BACK, OUT_BACK, IN_OUT_BACK,
    IN_BOUNCE, OUT_BOUNCE, IN_OUT_BOUNCE,
    IN_ELASTIC, OUT_ELASTIC, OUT_ELASTIC_HALF, OUT_ELASTIC_QUARTER, IN_OUT_ELASTIC
};

/** Number of AnimationEquation values (Java: values().length). */
constexpr int ANIMATION_EQUATION_COUNT = static_cast<int>(AnimationEquation::IN_OUT_ELASTIC) + 1;

/** Calculates a new t value using the animation equation. t and the result are in [0,1]. */
float calc(AnimationEquation eq, float t);

}  // namespace opsu
