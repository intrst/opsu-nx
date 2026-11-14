#pragma once
#include "opsu/ui/animations/AnimationEquation.h"

namespace opsu {

/**
 * A show / hide transition driven by an easing curve (never linear). Port of EasedSlide.java.
 * Progress runs 0 (hidden) .. 1 (shown); get() maps it through the easing equation.
 */
class EasedSlide {
public:
    EasedSlide(int openMs, int closeMs, AnimationEquation eqn);

    void open() { opening = true; }
    void close() { opening = false; }
    /** Jumps to the fully shown or fully hidden state, without animating. */
    void snap(bool shown);
    /** Advances the transition. Returns true if the progress changed. */
    bool update(int delta);
    /** Eased progress, from 0 (hidden) to 1 (shown). */
    float get() const;
    bool isOpening() const { return opening; }
    bool isVisible() const { return p > 0.f; }
    bool isIdle() const { return opening ? p >= 1.f : p <= 0.f; }

private:
    int openMs, closeMs;
    AnimationEquation eqn;
    float p = 0.f;
    bool opening = false;
};

}  // namespace opsu
