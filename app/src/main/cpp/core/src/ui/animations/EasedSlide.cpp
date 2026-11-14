#include "opsu/ui/animations/EasedSlide.h"
#include "opsu/util/JavaMath.h"

namespace opsu {

EasedSlide::EasedSlide(int openMs, int closeMs, AnimationEquation eqn)
    : openMs(java::max(1, openMs)), closeMs(java::max(1, closeMs)), eqn(eqn) {}

void EasedSlide::snap(bool shown) {
    opening = shown;
    p = shown ? 1.f : 0.f;
}

bool EasedSlide::update(int delta) {
    if (isIdle()) return false;
    float old = p;
    if (opening)
        p = java::min(1.f, p + static_cast<float>(delta) / static_cast<float>(openMs));
    else
        p = java::max(0.f, p - static_cast<float>(delta) / static_cast<float>(closeMs));
    return p != old;
}

float EasedSlide::get() const { return (p <= 0.f) ? 0.f : (p >= 1.f) ? 1.f : calc(eqn, p); }

}  // namespace opsu
