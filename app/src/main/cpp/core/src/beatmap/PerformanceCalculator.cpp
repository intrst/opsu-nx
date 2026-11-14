#include "opsu/beatmap/PerformanceCalculator.h"
#include <cmath>
#include <cstdint>
#include <exception>
#include "opsu/GameMod.h"
#include "opsu/beatmap/BeatmapDifficultyCalculator.h"
#include "opsu/logging/Log.h"
#include "opsu/util/JavaMath.h"

namespace opsu {
namespace PerformanceCalculator {

namespace {
// Java int arithmetic wraps; signed overflow is UB in C++.
int32_t wadd(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
int32_t wsub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

/** Converts a skill's star value to its base pp. */
double starsToPp(double stars) {
    return std::pow(5.0 * java::max(1.0, stars / 0.0675) - 4.0, 3.0) / 100000.0;
}

void compute(Result& r, bool max, double aimStars, double speedStars, float od, float ar,
             int h300, int h100, int h50, int miss, int combo, int maxCombo,
             int totalHits, int objectCount, int mods) {
    double accuracy = (h300 * 300.0 + h100 * 100.0 + h50 * 50.0) / (totalHits * 300.0);
    bool hidden = (mods & GameMod::HIDDEN.getBit()) != 0;
    bool flashlight = (mods & GameMod::FLASHLIGHT.getBit()) != 0;

    double lengthBonus = 0.95 + 0.4 * java::min(1.0, totalHits / 2000.0) +
                         ((totalHits > 2000) ? std::log10(totalHits / 2000.0) * 0.5 : 0.0);
    double missPenalty = std::pow(0.97, miss);
    double comboScale = (maxCombo > 0) ? java::min(std::pow((double)combo / maxCombo, 0.8), 1.0) : 1.0;

    // aim
    double aim = starsToPp(aimStars) * lengthBonus * missPenalty * comboScale;
    double arBonus = 0;
    if (ar > 10.33)
        arBonus += 0.3 * (ar - 10.33);
    else if (ar < 8.0)
        arBonus += 0.01 * (8.0 - ar);
    aim *= 1.0 + java::min(arBonus, arBonus * totalHits / 1000.0);
    if (hidden)
        aim *= 1.18;
    aim *= 0.5 + accuracy / 2.0;
    aim *= 0.98 + (od * od) / 2500.0;   // od * od is a float product, widened afterwards (as in Java)

    // speed
    double speed = starsToPp(speedStars) * lengthBonus * missPenalty * comboScale;
    speed *= 0.95 + (od * od) / 750.0;
    speed *= std::pow(accuracy, (14.5 - java::max((double)od, 8.0)) / 2.0);

    // accuracy
    double betterAccuracy = java::max(
        0.0, ((wsub(h300, wsub(totalHits, objectCount))) * 6.0 + h100 * 2.0 + h50) / (objectCount * 6.0));
    double acc = std::pow(1.52163, od) * std::pow(betterAccuracy, 24) * 2.83;
    acc *= java::min(1.15, std::pow(objectCount / 1000.0, 0.3));
    if (hidden)
        acc *= 1.08;
    if (flashlight)
        acc *= 1.02;

    // reading: grows with approach rate, amplified by hidden/flashlight, hurt by misses and combo loss
    double reading = std::pow(java::max(0.0, ar - 3.0), 1.8) * 0.9 * lengthBonus * missPenalty * comboScale;
    if (hidden)
        reading *= 1.25;
    if (flashlight)
        reading *= 1.15;
    reading *= 0.5 + accuracy / 2.0;

    // combine
    double multiplier = 1.12;
    if ((mods & GameMod::NO_FAIL.getBit()) != 0)
        multiplier *= 0.90;
    if ((mods & GameMod::SPUN_OUT.getBit()) != 0)
        multiplier *= 0.95;
    double total = std::pow(
        std::pow(aim, 1.1) + std::pow(speed, 1.1) + std::pow(acc, 1.1) + std::pow(reading, 1.1), 1.0 / 1.1
    ) * multiplier;

    if (max) {
        r.maxAim = aim;
        r.maxSpeed = speed;
        r.maxAccuracy = acc;
        r.maxReading = reading;
        r.maxTotal = total;
    } else {
        r.aim = aim;
        r.speed = speed;
        r.accuracy = acc;
        r.reading = reading;
        r.total = total;
    }
}
}  // namespace

float Result::ratio(double achieved, double max) {
    return (max <= 0) ? 0.f : (float)java::max(0.0, java::min(1.0, achieved / max));
}

Result calculateFromStars(double aimStars, double speedStars, const Beatmap& beatmap, int hit300, int hit100, int hit50,
                          int miss, int combo, int maxCombo, int mods) {
    Result r;
    if (aimStars < 0 || speedStars < 0)
        return r;

    int totalHits = wadd(wadd(hit300, hit100), wadd(hit50, miss));
    if (totalHits <= 0)
        return r;
    int objectCount = beatmap.objects ? (int)beatmap.objects->size() : totalHits;
    if (maxCombo <= 0)
        maxCombo = combo;

    float od = beatmap.overallDifficulty;
    float ar = (beatmap.approachRate >= 0) ? beatmap.approachRate : od;
    if ((mods & GameMod::HARD_ROCK.getBit()) != 0) {
        od = java::min(10.f, od * 1.4f);
        ar = java::min(10.f, ar * 1.4f);
    } else if ((mods & GameMod::EASY.getBit()) != 0) {
        od *= 0.5f;
        ar *= 0.5f;
    }

    // what was played, then a perfect play for the "maximum"
    compute(r, false, aimStars, speedStars, od, ar, hit300, hit100, hit50, miss, combo, maxCombo, totalHits, objectCount, mods);
    compute(r, true, aimStars, speedStars, od, ar, totalHits, 0, 0, 0, maxCombo, maxCombo, totalHits, objectCount, mods);
    return r;
}

Result calculate(const StarsProvider& stars, const Beatmap& beatmap, int hit300, int hit100, int hit50, int miss,
                 int combo, int maxCombo, int mods) {
    double aimStars = 0, speedStars = 0;
    try {
        stars(beatmap, aimStars, speedStars);
    } catch (const std::exception& e) {
        Log::error("Failed to compute difficulty for the performance breakdown.", e);
        return Result();
    } catch (...) {
        Log::error("Failed to compute difficulty for the performance breakdown.");
        return Result();
    }
    return calculateFromStars(aimStars, speedStars, beatmap, hit300, hit100, hit50, miss, combo, maxCombo, mods);
}

Result calculate(const Beatmap& beatmap, int hit300, int hit100, int hit50, int miss, int combo, int maxCombo, int mods) {
    StarsProvider provider = [](const Beatmap& b, double& aimStars, double& speedStars) {
        BeatmapDifficultyCalculator c(b);
        c.calculate();
        aimStars = c.getStars(BeatmapDifficultyCalculator::DIFFICULTY_AIM);
        speedStars = c.getStars(BeatmapDifficultyCalculator::DIFFICULTY_SPEED);
    };
    return calculate(provider, beatmap, hit300, hit100, hit50, miss, combo, maxCombo, mods);
}

}  // namespace PerformanceCalculator
}  // namespace opsu
