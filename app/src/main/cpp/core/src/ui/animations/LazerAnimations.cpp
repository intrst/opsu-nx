#include "opsu/ui/animations/LazerAnimations.h"
#include <cmath>
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaMath.h"

namespace opsu {
namespace LazerAnimations {
namespace {
inline float clamp01(float v) { return (v < 0.f) ? 0.f : (v > 1.f) ? 1.f : v; }
}  // namespace

using AE = AnimationEquation;

int fadeInTime(int approachTime) {
    return java::toInt(400.f * java::min(1.f, static_cast<float>(approachTime) / PREEMPT_MIN));
}

float approachScale(int timeDiff, int approachTime) {
    if (timeDiff <= 0 || approachTime <= 0) return 1.f;
    return 1.f + (APPROACH_START_SCALE - 1.f) * static_cast<float>(timeDiff) / static_cast<float>(approachTime);
}

float approachAlpha(int timeDiff, int approachTime, int fadeInTime) {
    float elapsed = static_cast<float>(approachTime - timeDiff);
    float fadeIn = java::min(static_cast<float>(fadeInTime) * 2.f, static_cast<float>(approachTime));
    float a = APPROACH_MAX_ALPHA * ((fadeIn > 0.f) ? clamp01(elapsed / fadeIn) : 1.f);
    if (timeDiff < 0) a *= clamp01(1.f + static_cast<float>(timeDiff) / APPROACH_FADE_OUT_TIME);
    return a;
}

float hitCircleProgress(float t) { return calc(AE::OUT_QUAD, clamp01(t / HIT_CIRCLE_TIME)); }
float hitCircleAlpha(float t) { return 1.f - clamp01(t / HIT_CIRCLE_TIME); }

const AnimationTrack JUDGEMENT_ALPHA = AnimationTrack(0.f)
    .to(1.f, 120.f, AE::LINEAR)
    .delay(380.f)
    .to(0.f, 600.f, AE::LINEAR);

const AnimationTrack JUDGEMENT_HIT_SCALE = AnimationTrack(0.6f)
    .to(1.1f, 96.f, AE::LINEAR)
    .delay(24.f)
    .to(0.9f, 24.f, AE::LINEAR)
    .set(0.95f)
    .to(1.f, 24.f, AE::LINEAR);

const AnimationTrack JUDGEMENT_MISS_SCALE = AnimationTrack(1.6f).to(1.f, 100.f, AE::IN_QUAD);
const AnimationTrack JUDGEMENT_MISS_Y = AnimationTrack(-5.f).to(75.f, 1100.f, AE::IN_QUAD);
const AnimationTrack JUDGEMENT_MISS_ROTATION = AnimationTrack(0.f)
    .to(1.f, 120.f, AE::LINEAR)
    .to(2.f, 980.f, AE::IN_QUAD);

const AnimationTrack LIGHTING_SCALE = AnimationTrack(0.8f).to(1.2f, 600.f, AE::OUT_QUAD);
const AnimationTrack LIGHTING_ALPHA = AnimationTrack(0.f)
    .to(1.f, 200.f, AE::LINEAR)
    .delay(200.f)
    .to(0.f, 1000.f, AE::LINEAR);

float followEndScale(float t) {
    return 1.f - 0.2f * AnimationTrack::progress(t, 0.f, FOLLOW_END_TIME, AE::OUT_QUAD);
}
float followEndAlpha(float t) { return 1.f - AnimationTrack::progress(t, 0.f, FOLLOW_END_TIME, AE::IN_QUAD); }

float reverseArrowScale(float msSinceBeat, float beatLength) {
    float duration = java::min(REVERSE_ARROW_PULSE_MAX_TIME, beatLength);
    if (duration <= 0.f || msSinceBeat < 0.f || msSinceBeat >= duration) return 1.f;
    float progress = calc(AE::OUT_QUAD, msSinceBeat / duration);
    return REVERSE_ARROW_PULSE_SCALE - (REVERSE_ARROW_PULSE_SCALE - 1.f) * progress;
}

float kiaiFlashAlpha(float msSinceBeat, float beatLength) {
    float duration = beatLength * KIAI_FLASH_FADE_RATIO;
    if (duration <= 0.f || msSinceBeat < 0.f || msSinceBeat >= duration) return 0.f;
    return KIAI_FLASH_OPACITY * (1.f - calc(AE::OUT_SINE, msSinceBeat / duration));
}

float damp(float current, float target, float base, float ms) {
    if (ms <= 0.f) return current;
    return current + (target - current) * (1.f - static_cast<float>(std::pow(static_cast<double>(base), static_cast<double>(ms))));
}

float spinnerAlpha(float sinceStart, float sinceEnd, float fadeInTime) {
    float half = fadeInTime / 2.f;
    float in = (half > 0.f) ? clamp01((sinceStart + half) / half) : 1.f;
    float out = 1.f - clamp01(sinceEnd / SPINNER_FADE_OUT_TIME);
    return in * out;
}

float spinnerApproachScale(float sinceStart, float duration) {
    float p = (duration > 0.f) ? clamp01(sinceStart / duration) : 1.f;
    return SPINNER_APPROACH_START + (SPINNER_APPROACH_END - SPINNER_APPROACH_START) * p;
}

int spinnerMetreBars(float progress, bool noBlink, bool completed, float random) {
    if (completed) return SPINNER_METRE_BARS;
    float p = java::min(99.f, progress * 100.f);
    int bars = java::toInt(p) / SPINNER_METRE_BARS;
    if (!noBlink && random < ((java::toInt(p) % SPINNER_METRE_BARS) / static_cast<float>(SPINNER_METRE_BARS))) bars++;
    return bars;
}

float spinnerNewStyleScale(float progress) { return 0.8f + 0.2f * calc(AE::OUT_QUAD, clamp01(progress)); }

float spinnerSpmHidden(float sinceStart, float sinceEnd) {
    float in = 1.f - AnimationTrack::progress(sinceStart, 0.f, SPINNER_SPM_SLIDE_TIME, AE::OUT_QUINT);
    float out = AnimationTrack::progress(sinceEnd, 0.f, SPINNER_SPM_SLIDE_TIME, AE::OUT_QUINT);
    return java::max(in, out);
}

float outQuad(float t, float duration) { return AnimationTrack::progress(t, 0.f, duration, AE::OUT_QUAD); }

}  // namespace LazerAnimations
}  // namespace opsu
