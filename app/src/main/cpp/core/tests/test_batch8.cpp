// Tests for batch 8 (PjskGameplay). Behavioural scenarios derived by hand from PjskGameplay.java.
#include <cstdio>
#include <vector>
#include "opsu/fourresonance/PjskGameplay.h"

using namespace opsu::fourresonance;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

using Note = PjskChart::Note;
using TM = PjskChart::TailMode;

static Note mk(int t, float lane, int endT = -1, TM tail = TM::RELEASE, bool crit = false) {
    Note n; n.lane = lane; n.size = 1.0f; n.timeMs = t; n.endTimeMs = endT < 0 ? t : endT;
    n.hold = endT >= 0; n.tail = tail; n.critical = crit; return n;
}
static int cnt(const PjskGameplay& g, Judgment j) { return g.counts[static_cast<size_t>(j)]; }

struct Ev { bool tail; Judgment j; double diff; int idx; };

int main() {
    // --- tap: perfect, hitbox edges, windows ---
    {
        PjskChart c; c.notes = {mk(1000, 0)};
        std::vector<Ev> evs;
        PjskGameplay g(c, [&](const PjskNote& n, bool tail, Judgment j, double d) {
            evs.push_back({tail, j, d, static_cast<int>(n.data - &c.notes[0])});
        });
        CHECK(g.totalJudgments() == 1 && !g.isFinished() && g.score() == 0);
        g.touchDown(0, 2.5, 1000);            // outside span 1 + leniency 1 -> nothing
        g.touchDown(0, 0, 1000 - 109);        // too early (good window 108.33)
        CHECK(evs.empty() && g.combo == 0 && !g.lastJudgment.has_value());
        g.touchDown(0, 2.0, 1010);            // inclusive edge of the hitbox
        CHECK(evs.size() == 1 && evs[0].j == Judgment::PERFECT && evs[0].diff == 10 && !evs[0].tail && evs[0].idx == 0);
        CHECK(g.combo == 1 && g.maxCombo == 1 && g.isFinished() && g.getFirstActive() == 1);
        CHECK(g.score() == 1000000 && cnt(g, Judgment::PERFECT) == 1 && g.lastJudgment == Judgment::PERFECT);
        g.touchDown(0, 0, 1010);              // already done: no double judgment
        CHECK(evs.size() == 1);
    }
    // --- tap: miss from update(), boundary is strictly greater than goodLate ---
    {
        PjskChart c; c.notes = {mk(1000, 0), mk(1500, 0)};
        PjskGameplay g(c);
        g.touchDown(0, 0, 1000 + 80);         // GREAT (late 80 > perfect 41.7, <= 83.3)
        CHECK(g.lastJudgment == Judgment::GREAT && g.combo == 1);
        g.update(1500 + 108.0);               // inside goodLate (108.33): not missed yet
        CHECK(!g.isFinished() && cnt(g, Judgment::MISS) == 0);
        g.update(1500 + 109.0);
        CHECK(g.isFinished() && cnt(g, Judgment::MISS) == 1 && g.combo == 0 && g.maxCombo == 1);
        // score: 10*0.7 + 0 of 20
        CHECK(g.score() == 350000);
    }
    // --- selection among overlapping notes ---
    {
        PjskChart c; c.notes = {mk(1000, 0), mk(1000, 3)};
        PjskGameplay g(c);
        g.touchDown(0, 2.8, 1000);            // note 1 outside by 1.8, note 0 by 1.8+... -> lane 3 note inside
        CHECK(g.getNotes()[1].state == PjskNote::State::DONE && g.getNotes()[0].state == PjskNote::State::WAITING);
        CHECK(g.getFirstActive() == 0);       // note 0 still active
    }
    {
        PjskChart c; c.notes = {mk(1000, 0), mk(1030, 0)};
        PjskGameplay g(c);
        g.touchDown(0, 0, 1040);              // older note (diff 40) beats newer (diff 10)
        CHECK(g.getNotes()[0].state == PjskNote::State::DONE && g.getNotes()[1].state == PjskNote::State::WAITING);
        CHECK(g.getNotes()[0].headJudgment == Judgment::PERFECT);  // 40 <= 41.67
    }
    // --- hold (RELEASE): perfect, early release, slide-off, never released ---
    {
        PjskChart c; c.notes = {mk(2000, 2, 3000)};
        PjskGameplay g(c);
        CHECK(g.totalJudgments() == 2);
        g.touchDown(7, 2, 2000);
        CHECK(g.getNotes()[0].state == PjskNote::State::HELD && g.getNotes()[0].pointer == 7 && !g.isFinished());
        g.touchUp(8, 3000);                   // other finger: ignored
        CHECK(g.getNotes()[0].state == PjskNote::State::HELD);
        g.touchUp(7, 3010);
        CHECK(g.isFinished() && g.getNotes()[0].tailJudgment == Judgment::PERFECT && g.getNotes()[0].pointer == -1);
        CHECK(g.combo == 2 && g.score() == 1000000);
    }
    {
        PjskChart c; c.notes = {mk(2000, 2, 3000)};
        PjskGameplay g(c);
        g.touchDown(0, 2, 2000);
        g.touchMove(0, 3.9, 2500);            // still within span(1..3) + 1
        CHECK(g.getNotes()[0].state == PjskNote::State::HELD);
        g.touchMove(0, 4.1, 2500);            // slid off, 500 ms early -> tail MISS
        CHECK(g.isFinished() && g.getNotes()[0].tailJudgment == Judgment::MISS && g.combo == 0 && g.maxCombo == 1);
        CHECK(g.score() == 500000);
    }
    {
        PjskChart c; c.notes = {mk(2000, 2, 3000)};
        PjskGameplay g(c);
        g.touchDown(0, 2, 2000);
        g.update(3000 + 141);                 // goodLate of SLIDE_END is 141.67: still held
        CHECK(g.getNotes()[0].state == PjskNote::State::HELD);
        g.update(3000 + 142);                 // never released -> MISS
        CHECK(g.isFinished() && g.getNotes()[0].tailJudgment == Judgment::MISS);
    }
    // --- missed hold head also misses the tail (2 misses) ---
    {
        PjskChart c; c.notes = {mk(2000, 2, 3000)};
        std::vector<Ev> evs;
        PjskGameplay g(c, [&](const PjskNote&, bool t, Judgment j, double d) { evs.push_back({t, j, d, 0}); });
        g.update(2200);
        CHECK(evs.size() == 2 && !evs[0].tail && evs[0].diff == 200 && evs[1].tail && evs[1].diff == 2200 - 3000);
        CHECK(cnt(g, Judgment::MISS) == 2 && g.isFinished());
    }
    // --- TRACE tail: perfect once reached while held ---
    {
        PjskChart c; c.notes = {mk(2000, 0, 3000, TM::TRACE)};
        PjskGameplay g(c);
        g.touchDown(0, 0, 2000);
        g.update(2999);
        CHECK(!g.isFinished());
        g.update(3000);
        CHECK(g.isFinished() && g.getNotes()[0].tailJudgment == Judgment::PERFECT && g.combo == 2);
    }
    // --- NONE tail: not judged; dropping early breaks combo, reaching the end does not ---
    {
        PjskChart c; c.notes = {mk(2000, 0, 3000, TM::NONE)};
        PjskGameplay g(c);
        CHECK(g.totalJudgments() == 1);
        g.touchDown(0, 0, 2000);
        g.update(3000);
        CHECK(g.isFinished() && !g.getNotes()[0].tailJudgment.has_value() && g.combo == 1);
    }
    {
        PjskChart c; c.notes = {mk(2000, 0, 3000, TM::NONE)};
        PjskGameplay g(c);
        g.touchDown(0, 0, 2000);
        g.touchUp(0, 2500);                   // 500 ms early (> 125 ms) -> combo reset, no judgment
        CHECK(g.isFinished() && g.combo == 0 && g.maxCombo == 1 && cnt(g, Judgment::MISS) == 0);
        CHECK(g.score() == 1000000);          // total weight is 10 (tail not counted), head was perfect
    }
    // --- critical head uses the critical window; score rounding ---
    {
        PjskChart c; c.notes = {mk(1000, 0, -1, TM::RELEASE, true), mk(2000, 0), mk(3000, 0)};
        PjskGameplay g(c);
        g.touchDown(0, 0, 1000 + 60);         // normal: GREAT; critical (perfect 55): GREAT too (60 > 55, <= 75)
        CHECK(g.lastJudgment == Judgment::GREAT);
        g.touchDown(0, 0, 2000);
        g.touchDown(0, 0, 3000);
        // earned 27 of 30 -> 900000
        CHECK(g.score() == 900000);
    }
    {
        PjskChart c; c.notes = {mk(1000, 0), mk(2000, 0), mk(3000, 0)};
        PjskGameplay g(c);
        g.touchDown(0, 0, 1060); g.touchDown(0, 0, 2000);  // GREAT + PERFECT = 17 of 30 earned once 3rd missed
        g.update(5000);
        CHECK(g.score() == 566667);           // round(1e6 * 17 / 30) = 566666.67 -> 566667
    }
    if (failures == 0) std::printf("batch8: all passed\n");
    return failures ? 1 : 0;
}
