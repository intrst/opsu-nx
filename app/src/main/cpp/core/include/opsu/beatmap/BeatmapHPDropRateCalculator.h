#pragma once
#include "opsu/beatmap/Beatmap.h"

namespace opsu {

/**
 * osu!'s HP drop rate algorithm (peppy, ppy/osu-iPhone:OsuFiletype.m).
 * Port of itdelatrisu.opsu.beatmap.BeatmapHPDropRateCalculator.
 *
 * SEAM: the Java constructor loads missing timing points (BeatmapDB.load) and parses the hit objects
 * (BeatmapParser.parseHitObjects). Those need the DB / file parser, so here the caller must pass a
 * beatmap whose `timingPoints` and `objects` are already loaded; calculate() throws otherwise.
 * The beatmap is referenced, not copied: it must outlive the calculator.
 */
class BeatmapHPDropRateCalculator {
public:
    BeatmapHPDropRateCalculator(const Beatmap& beatmap, float hpDrainRate, float overallDifficulty)
        : beatmap_(beatmap), hpDrainRate_(hpDrainRate), overallDifficulty_(overallDifficulty) {}

    float getHpDropRate() const { return hpDropRate_; }
    float getHpMultiplierNormal() const { return hpMultiplierNormal_; }
    float getHpMultiplierComboEnd() const { return hpMultiplierComboEnd_; }

    /**
     * Calculates the HP drop rate for the beatmap.
     * @throws std::logic_error if timing points or objects are not loaded,
     *         std::out_of_range if there are no objects (Java: ArrayIndexOutOfBoundsException).
     */
    void calculate();

private:
    const Beatmap& beatmap_;
    float hpDrainRate_;
    float overallDifficulty_;
    float hpDropRate_ = 0;
    float hpMultiplierNormal_ = 0;
    float hpMultiplierComboEnd_ = 0;
};

}  // namespace opsu
