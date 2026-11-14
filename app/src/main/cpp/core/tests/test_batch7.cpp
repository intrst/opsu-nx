// Tests for batch 7 (PjskJudge, PjskChart, PjskNote, OsuToPjsk time<->beat helpers).
// Behavioural, no JVM golden (no javac on the box). Expected numbers were computed in Python
// from the Java source formulas.
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include "opsu/fourresonance/OsuToPjsk.h"
#include "opsu/fourresonance/PjskChart.h"
#include "opsu/fourresonance/PjskJudge.h"
#include "opsu/fourresonance/PjskNote.h"

using namespace opsu::fourresonance;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

int main() {
    // ---- Judgment ----
    CHECK(multiplier(Judgment::PERFECT) == 1.0 && multiplier(Judgment::GREAT) == 0.7);
    CHECK(multiplier(Judgment::GOOD) == 0.5 && multiplier(Judgment::MISS) == 0.0);
    CHECK(static_cast<int>(Judgment::MISS) == 3);

    // ---- windows (exact frame->ms values) ----
    CHECK(near(TAP_NORMAL.perfectEarly, 41.66666666666667) && near(TAP_NORMAL.greatLate, 83.33333333333334));
    CHECK(near(TAP_NORMAL.goodEarly, 108.33333333333334) && near(TAP_NORMAL.goodLate, 108.33333333333334));
    CHECK(near(SLIDE_END.perfectEarly, 58.333333333333336) && near(SLIDE_END.perfectLate, 66.66666666666667));
    CHECK(near(SLIDE_END.greatEarly, 108.33333333333334) && near(SLIDE_END.greatLate, 133.33333333333334));
    CHECK(near(SLIDE_END.goodEarly, 125.0) && near(SLIDE_END.goodLate, 141.66666666666669));
    // trace: great and good default to perfect
    CHECK(near(SLIDE_END_TRACE.perfectEarly, 108.33333333333334) && near(SLIDE_END_TRACE.perfectLate, 133.33333333333334));
    CHECK(SLIDE_END_TRACE.greatEarly == SLIDE_END_TRACE.perfectEarly && SLIDE_END_TRACE.goodLate == SLIDE_END_TRACE.perfectLate);
    CHECK(near(TAP_CRITICAL.perfectEarly, 3.3 * 1000.0 / 60) && near(TAP_CRITICAL.greatEarly, 4.5 * 1000.0 / 60));

    // judge(): nested, inclusive at the edges, early = negative
    CHECK(TAP_NORMAL.judge(0) == Judgment::PERFECT);
    CHECK(TAP_NORMAL.judge(-TAP_NORMAL.perfectEarly) == Judgment::PERFECT);
    CHECK(TAP_NORMAL.judge(TAP_NORMAL.perfectLate) == Judgment::PERFECT);
    CHECK(TAP_NORMAL.judge(TAP_NORMAL.perfectLate + 0.001) == Judgment::GREAT);
    CHECK(TAP_NORMAL.judge(-TAP_NORMAL.greatEarly) == Judgment::GREAT);
    CHECK(TAP_NORMAL.judge(-TAP_NORMAL.greatEarly - 0.001) == Judgment::GOOD);
    CHECK(TAP_NORMAL.judge(TAP_NORMAL.goodLate) == Judgment::GOOD);
    CHECK(TAP_NORMAL.judge(TAP_NORMAL.goodLate + 0.001) == Judgment::MISS);
    CHECK(TAP_NORMAL.inRange(108.0) && !TAP_NORMAL.inRange(109.0) && !TAP_NORMAL.inRange(-109.0));
    // asymmetric window
    CHECK(SLIDE_END.judge(62.0) == Judgment::PERFECT && SLIDE_END.judge(-62.0) == Judgment::GREAT);
    CHECK(SLIDE_END.judge(141.0) == Judgment::GOOD && SLIDE_END.judge(-141.0) == Judgment::MISS);
    CHECK(NOTE_WEIGHT == 10 && MAX_SCORE == 1000000);
    // constexpr works (header-only constants)
    static_assert(TAP_NORMAL.judge(0) == Judgment::PERFECT, "constexpr judge");

    // ---- time <-> beat ----
    std::vector<PjskChart::BpmSegment> none;
    CHECK(OsuToPjsk::timeMsToBeat(1000, none) == 2.0 && OsuToPjsk::beatToTimeMs(3, none) == 1500.0);
    std::vector<PjskChart::BpmSegment> segs = {
        {0, 0, 500, 120},       // 120 bpm
        {2000, 4, 250, 240},    // 240 bpm from beat 4
    };
    CHECK(near(OsuToPjsk::timeMsToBeat(1000, segs), 2.0));
    CHECK(near(OsuToPjsk::timeMsToBeat(2000, segs), 4.0));
    CHECK(near(OsuToPjsk::timeMsToBeat(2500, segs), 6.0));
    CHECK(near(OsuToPjsk::timeMsToBeat(1999.9999995, segs), 4.0, 1e-7));  // inside the 1e-6 tolerance
    CHECK(near(OsuToPjsk::timeMsToBeat(-100, segs), -0.2));         // before first segment: uses first
    CHECK(near(OsuToPjsk::beatToTimeMs(2, segs), 1000.0));
    CHECK(near(OsuToPjsk::beatToTimeMs(4, segs), 2000.0));
    CHECK(near(OsuToPjsk::beatToTimeMs(6, segs), 2500.0));

    // ---- PjskChart ----
    PjskChart chart;
    CHECK(chart.scaledTime(1234.0) == 1234.0);  // no timeScale -> identity
    chart.bpmSegments = segs;
    CHECK(near(chart.timeToBeat(2500), 6.0) && near(chart.beatToTime(6), 2500.0));
    CHECK(PjskChart::STAGE_WIDTH == 12.0f);
    chart.timeScale = {{0, 0, 1.0}, {2, 1000, 2.0}, {4, 2000, 0.5}};
    const double ts[] = {-100, 0, 500, 1000, 1500, 2000, 3000};
    const double ex[] = {-100, 0, 500, 1000, 2000, 3000, 3500};
    for (int i = 0; i < 7; i++) CHECK(near(chart.scaledTime(ts[i]), ex[i]));
    // lazy cache is never rebuilt (Java quirk): growing timeScale afterwards breaks lookups past the old end
    chart.timeScale.push_back({6, 4000, 3.0});
    bool threw = false;
    try { chart.scaledTime(4500); } catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
    // defaults
    PjskChart::Note n;
    CHECK(n.tail == PjskChart::TailMode::RELEASE && !n.hold && !n.critical && n.column == 0);

    // ---- PjskNote ----
    PjskChart::Note tap;
    tap.lane = 1.5f; tap.size = 0.5f; tap.timeMs = 1000; tap.endTimeMs = 1000;
    PjskNote t(tap);
    CHECK(t.headMs == 1000.0 && t.tailMs == 1000.0 && t.laneL == 1.0 && t.laneR == 2.0);
    CHECK(t.state == PjskNote::State::WAITING && t.pointer == -1);
    CHECK(!t.headJudgment.has_value() && !t.tailJudgment.has_value());
    CHECK(!t.isHold() && !t.hasTailJudgment());
    CHECK(&t.headWindow() == &TAP_NORMAL);
    CHECK(t.containsLane(1.0, 0) && t.containsLane(2.0, 0) && !t.containsLane(2.01, 0) && t.containsLane(2.2, 0.25));

    PjskChart::Note hold;
    hold.hold = true; hold.critical = true; hold.lane = -2.0f; hold.size = 1.0f;
    hold.timeMs = 500; hold.endTimeMs = 1500; hold.tail = PjskChart::TailMode::TRACE;
    PjskNote h(hold);
    CHECK(h.isHold() && h.hasTailJudgment() && h.tailMs == 1500.0);
    CHECK(&h.headWindow() == &TAP_CRITICAL && &h.tailWindow() == &SLIDE_END_TRACE);
    hold.tail = PjskChart::TailMode::NONE;
    CHECK(!h.hasTailJudgment() && &h.tailWindow() == &SLIDE_END);
    // float lane arithmetic: 0.1f - 0.3f is done in float, then widened
    PjskChart::Note fl; fl.lane = 0.1f; fl.size = 0.3f;
    PjskNote f(fl);
    CHECK(f.laneL == static_cast<double>(0.1f - 0.3f));
    CHECK(f.laneR == static_cast<double>(0.1f + 0.3f));
    h.headJudgment = Judgment::GREAT;
    CHECK(h.headJudgment.value() == Judgment::GREAT);

    if (failures == 0) std::printf("batch7: all passed\n");
    return failures ? 1 : 0;
}
