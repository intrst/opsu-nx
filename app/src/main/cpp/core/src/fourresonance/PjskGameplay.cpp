#include "opsu/fourresonance/PjskGameplay.h"
#include <algorithm>
#include "opsu/util/JavaMath.h"

namespace opsu {
namespace fourresonance {

namespace {
/** Time (ms) worth one lane unit of distance when choosing between overlapping notes (Sekai: 0.05 s). */
constexpr double TIME_PER_LANE_MS = 50;
}

PjskGameplay::PjskGameplay(const PjskChart& chart, Listener l) : listener(std::move(l)) {
    notes.reserve(chart.notes.size());
    double total = 0;
    for (const PjskChart::Note& cn : chart.notes) {
        notes.emplace_back(cn);
        total += NOTE_WEIGHT;
        if (notes.back().hasTailJudgment()) total += NOTE_WEIGHT;
    }
    totalWeight = total;
}

int PjskGameplay::score() const {
    return totalWeight <= 0 ? 0 : static_cast<int>(java::round(MAX_SCORE * earned / totalWeight));
}

int PjskGameplay::totalJudgments() const {
    int n = 0;
    for (const PjskNote& note : notes) n += note.hasTailJudgment() ? 2 : 1;
    return n;
}

void PjskGameplay::touchDown(int pointer, double lane, double nowMs) {
    PjskNote* best = nullptr;
    double bestScore = 0;
    for (size_t i = static_cast<size_t>(firstActive); i < notes.size(); i++) {
        PjskNote& n = notes[i];
        double diff = nowMs - n.headMs;
        if (diff < -n.headWindow().goodEarly) {
            // notes are sorted by head time, but windows differ slightly: keep scanning a bit
            if (-diff > 200) break;
            continue;
        }
        // the hitbox is the note's span widened by the leniency (a NaN lane never matches)
        if (n.state != PjskNote::State::WAITING || !n.headWindow().inRange(diff) ||
            !n.containsLane(lane, PjskStage::HITBOX_LENIENCY))
            continue;
        // Sekai scoring: closer to the note's span is better, and so is an older note (diff > 0)
        double outside = std::max(0.0, std::max(n.laneL - lane, lane - n.laneR));
        double score = -outside + diff / TIME_PER_LANE_MS;
        if (best == nullptr || score > bestScore) {
            best = &n;
            bestScore = score;
        }
    }
    if (best == nullptr) return;

    double diff = nowMs - best->headMs;
    Judgment j = best->headWindow().judge(diff);
    best->headJudgment = j;
    judged(*best, false, j, diff);
    if (best->isHold()) {
        best->state = PjskNote::State::HELD;
        best->pointer = pointer;
        held.push_back(best);
    } else {
        best->state = PjskNote::State::DONE;
        advance();
    }
}

void PjskGameplay::touchMove(int pointer, double lane, double nowMs) {
    for (int i = static_cast<int>(held.size()) - 1; i >= 0; i--) {
        PjskNote* n = held[static_cast<size_t>(i)];
        if (n->pointer == pointer && !n->containsLane(lane, HOLD_LANE_TOLERANCE)) release(*n, nowMs);
    }
    advance();
}

void PjskGameplay::touchUp(int pointer, double nowMs) {
    for (int i = static_cast<int>(held.size()) - 1; i >= 0; i--) {
        PjskNote* n = held[static_cast<size_t>(i)];
        if (n->pointer == pointer) release(*n, nowMs);
    }
    advance();
}

void PjskGameplay::update(double nowMs) {
    // missed heads
    for (size_t i = static_cast<size_t>(firstActive); i < notes.size(); i++) {
        PjskNote& n = notes[i];
        if (n.headMs - nowMs > 500) break;  // everything further is in the future
        if (n.state == PjskNote::State::WAITING && nowMs - n.headMs > n.headWindow().goodLate) {
            n.headJudgment = Judgment::MISS;
            judged(n, false, Judgment::MISS, nowMs - n.headMs);
            if (n.hasTailJudgment()) {
                n.tailJudgment = Judgment::MISS;
                judged(n, true, Judgment::MISS, nowMs - n.tailMs);
            }
            n.state = PjskNote::State::DONE;
        }
    }
    // held tails (backwards: finishTail removes from `held`)
    for (int i = static_cast<int>(held.size()) - 1; i >= 0; i--) {
        PjskNote* n = held[static_cast<size_t>(i)];
        const Window& w = n->tailWindow();
        if (n->data->tail == PjskChart::TailMode::TRACE) {
            if (nowMs >= n->tailMs) finishTail(*n, Judgment::PERFECT, nowMs - n->tailMs);
        } else if (n->data->tail == PjskChart::TailMode::NONE) {
            if (nowMs >= n->tailMs) finishTail(*n, std::nullopt, 0);
        } else if (nowMs - n->tailMs > w.goodLate) {
            finishTail(*n, Judgment::MISS, nowMs - n->tailMs);  // never released
        }
    }
    advance();
}

void PjskGameplay::release(PjskNote& n, double nowMs) {
    double diff = nowMs - n.tailMs;
    const Window& w = n.tailWindow();
    if (n.data->tail == PjskChart::TailMode::NONE) {
        // the tail is not judged, but dropping the hold well before its end breaks the combo
        if (diff < -SLIDE_END.goodEarly) combo = 0;
        finishTail(n, std::nullopt, diff);
        return;
    }
    Judgment j = diff < -w.goodEarly ? Judgment::MISS : w.judge(diff);
    finishTail(n, j, diff);
}

void PjskGameplay::finishTail(PjskNote& n, std::optional<Judgment> j, double diff) {
    held.erase(std::remove(held.begin(), held.end(), &n), held.end());
    n.pointer = -1;
    n.state = PjskNote::State::DONE;
    if (j.has_value()) {
        n.tailJudgment = j;
        judged(n, true, *j, diff);
    }
}

void PjskGameplay::judged(PjskNote& n, bool tail, Judgment j, double diff) {
    counts[static_cast<size_t>(j)]++;
    lastJudgment = j;
    earned += NOTE_WEIGHT * multiplier(j);
    if (j == Judgment::MISS) {
        combo = 0;
    } else {
        combo++;
        if (combo > maxCombo) maxCombo = combo;
    }
    if (listener) listener(n, tail, j, diff);
}

void PjskGameplay::advance() {
    while (firstActive < static_cast<int>(notes.size()) &&
           notes[static_cast<size_t>(firstActive)].state == PjskNote::State::DONE)
        firstActive++;
}

}  // namespace fourresonance
}  // namespace opsu
