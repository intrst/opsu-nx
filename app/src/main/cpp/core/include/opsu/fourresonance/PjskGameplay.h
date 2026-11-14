#pragma once
#include <array>
#include <functional>
#include <optional>
#include <vector>
#include "opsu/fourresonance/PjskChart.h"
#include "opsu/fourresonance/PjskJudge.h"
#include "opsu/fourresonance/PjskNote.h"
#include "opsu/fourresonance/PjskStage.h"

namespace opsu {
namespace fourresonance {

/**
 * Touch-driven gameplay logic for the PJSK mode (no rendering). Port of fourresonance/PjskGameplay.java.
 *
 * Feed it touch events with the lane under the finger and call update() every frame. All times are
 * ms on the audio timeline. Tap/hold head: judged on touch-down against the best note in range.
 * Hold: kept while the finger stays within the note's lane span (+ HOLD_LANE_TOLERANCE).
 * Tail: RELEASE must be released inside the tail window; TRACE is perfect once the tail time is reached
 * while held; NONE is not judged. Not ported (also not in Java): hidden slide ticks, consecutive-great
 * bonus, life/damage notes.
 *
 * Seams: the Listener interface is a std::function. `chart.notes` is copied into PjskNote objects that
 * point at the chart's Notes, so the chart must outlive the gameplay and its `notes` must not
 * reallocate. The object is not copyable/movable (the `held` list points into `notes`).
 * getNotes() returns the vector (Java: PjskNote[]); it is never resized after construction.
 * Nullable Java `lastJudgment` is std::optional.
 */
class PjskGameplay {
public:
    /** How far (in lane units) a finger holding a note may stray outside the note's lane span. */
    static constexpr double HOLD_LANE_TOLERANCE = PjskStage::HITBOX_LENIENCY;

    /** Receives judgment events: (note, tail = hold tail judgment, judgment, diffMs = actual - target). */
    using Listener = std::function<void(const PjskNote&, bool tail, Judgment, double diffMs)>;

    int combo = 0;     ///< Current combo.
    int maxCombo = 0;  ///< Highest combo reached.
    /** Judgment counts indexed by static_cast<int>(Judgment). */
    std::array<int, 4> counts{};
    std::optional<Judgment> lastJudgment;  ///< Last judgment made (empty if none yet).

    /** `listener` may be empty (Java null). */
    explicit PjskGameplay(const PjskChart& chart, Listener listener = nullptr);
    PjskGameplay(const PjskGameplay&) = delete;
    PjskGameplay& operator=(const PjskGameplay&) = delete;

    /** All notes (sorted by head time). */
    std::vector<PjskNote>& getNotes() { return notes; }
    const std::vector<PjskNote>& getNotes() const { return notes; }

    /** Index of the first note that is not done (for render culling). */
    int getFirstActive() const { return firstActive; }

    /** Current score (0 to MAX_SCORE). */
    int score() const;

    /** Number of judgments that count toward the max combo. */
    int totalJudgments() const;

    /** Whether every note is done. */
    bool isFinished() const { return firstActive >= static_cast<int>(notes.size()); }

    /** A finger touched the screen at `lane`. */
    void touchDown(int pointer, double lane, double nowMs);
    /** A finger moved to `lane`. */
    void touchMove(int pointer, double lane, double nowMs);
    /** A finger was lifted. */
    void touchUp(int pointer, double nowMs);

    /** Advances time: misses notes that ran out of window and resolves held tails. */
    void update(double nowMs);

private:
    std::vector<PjskNote> notes;
    std::vector<PjskNote*> held;
    Listener listener;
    int firstActive = 0;
    double earned = 0;
    double totalWeight = 0;

    void release(PjskNote& n, double nowMs);
    void finishTail(PjskNote& n, std::optional<Judgment> j, double diff);
    void judged(PjskNote& n, bool tail, Judgment j, double diff);
    void advance();
};

}  // namespace fourresonance
}  // namespace opsu
