// Tests for batch 9 (PjskAuto, java::compare(double,double)).
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include "opsu/fourresonance/PjskAuto.h"
#include "opsu/util/JavaMath.h"

using namespace opsu::fourresonance;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
using Note = PjskChart::Note;
using TM = PjskChart::TailMode;

static Note mk(int t, float lane, int endT = -1, TM tail = TM::RELEASE) {
    Note n; n.lane = lane; n.size = 1.0f; n.timeMs = t; n.endTimeMs = endT < 0 ? t : endT;
    n.hold = endT >= 0; n.tail = tail; return n;
}
static int cnt(const PjskGameplay& g, Judgment j) { return g.counts[static_cast<size_t>(j)]; }

int main() {
    // Double.compare semantics
    namespace J = opsu::java;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(J::compare(1.0, 2.0) == -1 && J::compare(2.0, 1.0) == 1 && J::compare(3.0, 3.0) == 0);
    CHECK(J::compare(-0.0, 0.0) == -1 && J::compare(0.0, -0.0) == 1);
    CHECK(J::compare(nan, 1e300) == 1 && J::compare(1e300, nan) == -1 && J::compare(nan, nan) == 0);

    // full autoplay of a mixed chart = every judgment PERFECT, score 1,000,000
    {
        PjskChart c;
        c.notes = {mk(1000, -3), mk(1500, 0, 2500), mk(2000, 3), mk(3000, 1, 4000, TM::TRACE),
                   mk(3500, -2, 4500, TM::NONE), mk(5000, 0)};
        PjskGameplay g(c);
        PjskAuto auto_(g);
        for (int t = 0; t <= 6000; t += 16) {   // 60 fps-ish frames, auto BEFORE gameplay
            auto_.update(t);
            g.update(t);
        }
        CHECK(g.isFinished());
        CHECK(cnt(g, Judgment::PERFECT) == g.totalJudgments() && cnt(g, Judgment::MISS) == 0);
        CHECK(g.combo == g.totalJudgments() && g.maxCombo == g.combo && g.score() == 1000000);
    }
    // touches are delivered at their exact times, not at the frame time
    {
        PjskChart c; c.notes = {mk(1000, 0)};
        double seen = -1;
        PjskGameplay g(c, [&](const PjskNote&, bool, Judgment, double diff) { seen = diff; });
        PjskAuto a(g);
        a.update(999);  CHECK(seen == -1);
        a.update(1007); CHECK(seen == 0);     // diff computed from the event time 1000, not 1007
    }
    // zero-length hold: down goes before up at the same time; shared time with another note's down
    {
        PjskChart c; c.notes = {mk(1000, 0, 1000), mk(1000, 5)};
        PjskGameplay g(c);
        PjskAuto a(g);
        a.update(1000);
        g.update(1000);
        CHECK(g.isFinished() && cnt(g, Judgment::PERFECT) == 3);
    }
    // empty chart is fine
    {
        PjskChart c;
        PjskGameplay g(c);
        PjskAuto a(g);
        a.update(1e9);
        CHECK(g.isFinished());
    }
    if (failures == 0) std::printf("batch9: all passed\n");
    return failures ? 1 : 0;
}
