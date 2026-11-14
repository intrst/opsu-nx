#pragma once
#include <string>

namespace opsu {

/**
 * Pure, renderer-free part of itdelatrisu.opsu.GameData: Grade, hit-result constants and the
 * score-percent / grade formulas. The rest of GameData (HUD, animations, score tracking) needs the
 * graphics backend and is ported later.
 */
namespace GameData {

/** Letter grades. (Java's per-grade images are not ported: use GameImage when the UI exists.) */
enum class Grade { NULL_ = 0, SS, SSH, S, SH, A, B, C, D };

/** Java's Grade.name() ("NULL" for NULL_). */
const char* gradeName(Grade g);

/** Hit result types. */
constexpr int HIT_MISS = 0, HIT_50 = 1, HIT_100 = 2, HIT_300 = 3, HIT_100K = 4, HIT_300K = 5, HIT_300G = 6,
              HIT_SLIDER10 = 7, HIT_SLIDER30 = 8, HIT_MAX = 9, HIT_SLIDER_REPEAT = 10, HIT_ANIMATION_RESULT = 11,
              HIT_SPINNERSPIN = 12, HIT_SPINNERBONUS = 13, HIT_MU = 14;

/** Hit object types, used for drawing results. */
enum class HitObjectType { CIRCLE, SLIDERTICK, SLIDER_FIRST, SLIDER_LAST, SPINNER };

/** Raw score percentage. */
float getScorePercent(int hit300, int hit100, int hit50, int miss);

/** Letter grade from hit counts, or Grade::NULL_ if no objects were processed. */
Grade getGrade(int hit300, int hit100, int hit50, int miss, bool silver);

}  // namespace GameData
}  // namespace opsu
