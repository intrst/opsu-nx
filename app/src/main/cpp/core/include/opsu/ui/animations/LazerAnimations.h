#pragma once
#include "opsu/ui/animations/AnimationEquation.h"
#include "opsu/ui/animations/AnimationTrack.h"

namespace opsu {

/**
 * The osu!lazer gameplay animations (legacy-skin components), as constants and allocation-free
 * functions of time. Port of LazerAnimations.java.
 * The AnimationTrack constants are `extern const` objects defined in LazerAnimations.cpp.
 */
namespace LazerAnimations {

// ---- hit circles
constexpr float PREEMPT_MIN = 450.f;
int fadeInTime(int approachTime);
constexpr float APPROACH_START_SCALE = 4.f;
constexpr float APPROACH_MAX_ALPHA = 0.9f;
constexpr float APPROACH_FADE_OUT_TIME = 50.f;
float approachScale(int timeDiff, int approachTime);
float approachAlpha(int timeDiff, int approachTime, int fadeInTime);
constexpr float HIT_CIRCLE_TIME = 240.f;
constexpr float HIT_CIRCLE_SCALE = 1.4f;
float hitCircleProgress(float t);
float hitCircleAlpha(float t);
constexpr float MISS_FADE_TIME = 100.f;

// ---- judgements (legacy)
constexpr float JUDGEMENT_TIME = 1100.f;
extern const AnimationTrack JUDGEMENT_ALPHA;
extern const AnimationTrack JUDGEMENT_HIT_SCALE;
extern const AnimationTrack JUDGEMENT_MISS_SCALE;
extern const AnimationTrack JUDGEMENT_MISS_Y;
extern const AnimationTrack JUDGEMENT_MISS_ROTATION;
constexpr float JUDGEMENT_MISS_MAX_ROTATION = 8.6f;

// ---- hit lighting
extern const AnimationTrack LIGHTING_SCALE;
extern const AnimationTrack LIGHTING_ALPHA;
constexpr float LIGHTING_TIME = 1400.f;

// ---- slider follow circle
constexpr int FOLLOW_INITIAL_TIME = 180;
constexpr float FOLLOW_ALPHA_TIME = 60.f;
constexpr float FOLLOW_END_TIME = 200.f;
float followEndScale(float t);
float followEndAlpha(float t);

// ---- reverse arrow
constexpr float REVERSE_ARROW_PULSE_SCALE = 1.3f;
constexpr float REVERSE_ARROW_PULSE_MAX_TIME = 300.f;
float reverseArrowScale(float msSinceBeat, float beatLength);

// ---- kiai flash
constexpr float KIAI_FLASH_OPACITY = 0.3f;
constexpr float KIAI_FLASH_FADE_RATIO = 0.75f;
float kiaiFlashAlpha(float msSinceBeat, float beatLength);

// ---- spinner (legacy skins)
constexpr float SPINNER_SPRITE_SCALE = 0.625f;
constexpr float SPINNER_TOP_OFFSET = 29.f;
constexpr float SPINNER_SPIN_Y = SPINNER_TOP_OFFSET + 335.f;
constexpr float SPINNER_CLEAR_Y = SPINNER_TOP_OFFSET + 115.f;
constexpr float SPINNER_BONUS_Y = SPINNER_TOP_OFFSET + 299.f;
constexpr float SPINNER_SPM_BG_X = -87.f, SPINNER_SPM_BG_Y = 445.f;
constexpr float SPINNER_SPM_COUNTER_X = 80.f, SPINNER_SPM_COUNTER_Y = 448.f;
constexpr float SPINNER_SPM_HIDE_OFFSET = 50.f;
constexpr float SPINNER_SPM_SLIDE_TIME = 500.f;
constexpr float SPINNER_METRE_TOP = 20.f;
constexpr int SPINNER_METRE_BARS = 10;
constexpr float SPINNER_APPROACH_START = 1.86f, SPINNER_APPROACH_END = 0.1f;
constexpr float SPINNER_FADE_OUT_TIME = 200.f;
constexpr float SPINNER_ROTATION_DAMP = 0.99f;
constexpr float SPINNER_SPINNING_THRESHOLD = 5.f;
constexpr int SPINNER_SPM_WINDOW = 595;
constexpr float SPINNER_BONUS_TIME = 800.f, SPINNER_BONUS_START_SCALE = 2.f, SPINNER_BONUS_END_SCALE = 1.28f;
constexpr float SPINNER_CLEAR_TIME = 400.f, SPINNER_CLEAR_START_SCALE = 2.f;
constexpr float SPINNER_SPIN_FADE_TIME = 300.f;
constexpr float SPINNER_GLOW_R = 3.f / 255.f, SPINNER_GLOW_G = 151.f / 255.f, SPINNER_GLOW_B = 1.f;

float damp(float current, float target, float base, float ms);
float spinnerAlpha(float sinceStart, float sinceEnd, float fadeInTime);
float spinnerApproachScale(float sinceStart, float duration);
int spinnerMetreBars(float progress, bool noBlink, bool completed, float random);
float spinnerNewStyleScale(float progress);
float spinnerSpmHidden(float sinceStart, float sinceEnd);
float outQuad(float t, float duration);

}  // namespace LazerAnimations
}  // namespace opsu
