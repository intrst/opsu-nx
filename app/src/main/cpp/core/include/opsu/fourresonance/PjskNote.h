#pragma once
#include <optional>
#include "opsu/fourresonance/PjskChart.h"
#include "opsu/fourresonance/PjskJudge.h"

namespace opsu {
namespace fourresonance {

/**
 * Runtime state of one note during play. Port of fourresonance/PjskNote.java.
 *
 * Seams: `data` is a non-owning pointer to the chart's Note (the chart's `notes` vector must outlive
 * this and not reallocate, same rule as Curve -> HitObject). Nullable Java Judgment fields are
 * std::optional (empty = null). Java's `Window` is returned by const reference to the constants.
 */
class PjskNote {
public:
    /** Note state. */
    enum class State : int {
        WAITING,  ///< Waiting for the head to be hit.
        HELD,     ///< Hold note whose head was hit and which is still being held.
        DONE      ///< Fully resolved (hit or missed).
    };

    const PjskChart::Note* data;                 ///< The converted chart note.
    State state = State::WAITING;                ///< Current state.
    std::optional<Judgment> headJudgment;        ///< Head judgment (empty until judged).
    std::optional<Judgment> tailJudgment;        ///< Tail judgment (empty until judged, or when the tail is not judged).
    int pointer = -1;                            ///< Pointer (finger) holding this note, or -1.
    double headMs;                               ///< Head time (ms).
    double tailMs;                               ///< Tail time (ms); equal to headMs for taps.
    double laneL;                                ///< Left lane edge (stage units).
    double laneR;                                ///< Right lane edge (stage units).

    explicit PjskNote(const PjskChart::Note& note);

    /** Whether this is a hold note. */
    bool isHold() const { return data->hold; }

    /** Whether the tail of this hold is judged. */
    bool hasTailJudgment() const { return data->hold && data->tail != PjskChart::TailMode::NONE; }

    /** The window for the head. */
    const Window& headWindow() const { return data->critical ? TAP_CRITICAL : TAP_NORMAL; }

    /** The window for the tail (NONE tails use the release window for early-release checks only). */
    const Window& tailWindow() const {
        return data->tail == PjskChart::TailMode::TRACE ? SLIDE_END_TRACE : SLIDE_END;
    }

    /** Whether `lane` is inside this note's lane span, widened by `tolerance` on both sides. */
    bool containsLane(double lane, double tolerance) const {
        return lane >= laneL - tolerance && lane <= laneR + tolerance;
    }
};

}  // namespace fourresonance
}  // namespace opsu
