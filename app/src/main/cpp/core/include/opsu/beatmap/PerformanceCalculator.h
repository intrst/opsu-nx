#pragma once
#include <functional>
#include "opsu/beatmap/Beatmap.h"

namespace opsu {

/**
 * Estimates performance points (pp) for a score, split into aim, speed, accuracy and reading parts.
 * Port of itdelatrisu.opsu.beatmap.PerformanceCalculator. All values are in pp.
 *
 * The Java `calculate` built a `BeatmapDifficultyCalculator` itself. `calculate(beatmap, ...)` below does the same; the
 * `StarsProvider` overload and `calculateFromStars` stay for callers that already have (or cache) the star values.
 * Like the calculator, the beatmap must have its timing points and hit objects loaded (otherwise: all zeroes + an error log).
 */
namespace PerformanceCalculator {

struct Result {
    /** The pp parts of the score that was actually played. */
    double aim = 0, speed = 0, accuracy = 0, reading = 0, total = 0;
    /** The pp parts of a perfect play (all 300s, full combo) with the same mods. */
    double maxAim = 0, maxSpeed = 0, maxAccuracy = 0, maxReading = 0, maxTotal = 0;

    /** achieved / maximum for a part, from 0 to 1 (0 when max <= 0). */
    static float ratio(double achieved, double max);
    float aimRatio() const { return ratio(aim, maxAim); }
    float speedRatio() const { return ratio(speed, maxSpeed); }
    float accuracyRatio() const { return ratio(accuracy, maxAccuracy); }
    float readingRatio() const { return ratio(reading, maxReading); }
};

/** Computes the aim and speed star values of a beatmap. May throw (Java: any Exception). */
using StarsProvider = std::function<void(const Beatmap&, double& aimStars, double& speedStars)>;

/**
 * Calculates the performance of a score.
 * @param maxCombo the highest combo possible (or <= 0 if unknown)
 * @param mods the mods used (bitmask of GameMod bits)
 * @return the result, or all zeroes if the beatmap's difficulty could not be computed (the provider threw; logged)
 */
Result calculate(const StarsProvider& stars, const Beatmap& beatmap, int hit300, int hit100, int hit50, int miss,
                 int combo, int maxCombo, int mods);

/** Java's `calculate(beatmap, ...)`: computes the stars with a BeatmapDifficultyCalculator, then as above. */
Result calculate(const Beatmap& beatmap, int hit300, int hit100, int hit50, int miss, int combo, int maxCombo, int mods);

/** Same as `calculate`, with the star values already known (all zeroes if either is negative). */
Result calculateFromStars(double aimStars, double speedStars, const Beatmap& beatmap, int hit300, int hit100, int hit50,
                          int miss, int combo, int maxCombo, int mods);

}  // namespace PerformanceCalculator
}  // namespace opsu
