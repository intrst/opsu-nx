#pragma once
#include <array>
#include <vector>
#include "opsu/beatmap/Beatmap.h"

namespace opsu {

/**
 * osu!tp's beatmap difficulty algorithm (Tom94, AiModtpDifficultyCalculator).
 * Port of itdelatrisu.opsu.beatmap.BeatmapDifficultyCalculator.
 *
 * SEAM (same as BeatmapHPDropRateCalculator): the Java constructor loaded missing timing points
 * (BeatmapDB.load) and parsed the hit objects (BeatmapParser.parseHitObjects). Here the caller must
 * pass a beatmap whose `timingPoints` and `objects` are already loaded; otherwise calculate() logs
 * an error and returns, leaving every value at -1 (exactly what Java does for an unloaded beatmap).
 * The beatmap is referenced, not copied: it must outlive the calculator.
 *
 * NOTE (inherited from the Java, kept bit-for-bit): the slider curves used here are built unscaled
 * but EqualDistanceMultiCurve still multiplies the pixel length by HitObject::getXMultiplier(), so
 * results depend on the last HitObject::init() call. See PORTING_NOTES.md ("Findings").
 */
class BeatmapDifficultyCalculator {
public:
    /** Difficulty types. */
    static constexpr int DIFFICULTY_SPEED = 0, DIFFICULTY_AIM = 1;

    /** circleSize = beatmap.circleSize, clockRate = 1. */
    explicit BeatmapDifficultyCalculator(const Beatmap& beatmap);

    /**
     * @param circleSize the circle size after mods / options
     * @param clockRate the clock rate (DT / HT / fixed speed); values <= 0 (and NaN) mean 1
     */
    BeatmapDifficultyCalculator(const Beatmap& beatmap, float circleSize, float clockRate);

    ~BeatmapDifficultyCalculator();  // out of line: TpHitObject is incomplete here
    BeatmapDifficultyCalculator(const BeatmapDifficultyCalculator&) = delete;
    BeatmapDifficultyCalculator& operator=(const BeatmapDifficultyCalculator&) = delete;

    double getStarRating() const { return starRating_; }
    /** @throws std::out_of_range for a type other than 0/1 (Java: ArrayIndexOutOfBoundsException). */
    double getDifficulty(int type) const { return difficulties_.at(static_cast<size_t>(type)); }
    double getStars(int type) const { return stars_.at(static_cast<size_t>(type)); }

    /**
     * Calculates the difficulty values and star ratings for the beatmap.
     * May throw what the slider curves throw (std::runtime_error for degenerate perfect curves, ...).
     */
    void calculate();

private:
    struct TpHitObject;  // strain helper (Java: tpHitObject / tpSlider)

    bool calculateStrainValues();
    double calculateDifficulty(int type) const;

    const Beatmap& beatmap_;
    float circleSize_;
    double clockRate_;
    std::vector<TpHitObject> tpHitObjects_;
    double starRating_ = -1;
    std::array<double, 2> difficulties_{{-1, -1}};
    std::array<double, 2> stars_{{-1, -1}};
};

}  // namespace opsu
