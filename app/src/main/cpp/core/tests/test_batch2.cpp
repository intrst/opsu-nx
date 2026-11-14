// Behavioural tests for batch 2 (HitObject, curves, GameMod, TimingPoint, Log, ...).
// Numeric/format fidelity against real Java is covered by test_golden.cpp.
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "opsu/GameMod.h"
#include "opsu/Utils.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/beatmap/TimingPoint.h"
#include "opsu/input/Keys.h"
#include "opsu/logging/Log.h"
#include "opsu/objects/curves/CatmullCurve.h"
#include "opsu/objects/curves/CircumscribedCircle.h"
#include "opsu/objects/curves/LinearBezier.h"
#include "opsu/replay/PlaybackSpeed.h"
#include "opsu/replay/ReplayFrame.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

template <class E, class F>
static bool throws(F f) { try { f(); } catch (const E&) { return true; } catch (...) { return false; } return false; }

struct CaptureLogger : ILogger {
    std::string last; int count = 0;
    void setSaveToFile(bool) override {}
    void rec(const std::string& t, const std::string& m) { last = t + "|" + m; count++; }
    void trace(const std::string& t, const std::string& m) override { rec(t, m); }
    void trace(const std::string& t, const std::string& m, const std::exception&) override { rec(t, m); }
    void debug(const std::string& t, const std::string& m) override { rec(t, m); }
    void debug(const std::string& t, const std::string& m, const std::exception&) override { rec(t, m); }
    void info(const std::string& t, const std::string& m) override { rec(t, m); }
    void info(const std::string& t, const std::string& m, const std::exception&) override { rec(t, m); }
    void warn(const std::string& t, const std::string& m) override { rec(t, m); }
    void warn(const std::string& t, const std::string& m, const std::exception&) override { rec(t, m); }
    void error(const std::string& t, const std::string& m) override { rec(t, m); }
    void error(const std::string& t, const std::string& m, const std::exception&) override { rec(t, m); }
};

int main() {
    // ---- Log ----
    CaptureLogger cl;
    Log::setLogger(&cl);
    Log::debug("hi");
    CHECK(cl.last == "Debug|hi");
    Log::warn("P", "msg");
    CHECK(cl.last == "Warn|[P] msg");
    Log::setLoggingLevel(Log::WARN);
    cl.count = 0;
    Log::info("dropped"); Log::debug("dropped");
    CHECK(cl.count == 0);
    Log::error("kept");
    CHECK(cl.count == 1);
    Log::setLoggingLevel(Log::ASSERT);   // >= ASSERT disables everything
    Log::error("dropped");
    CHECK(cl.count == 1);
    Log::setLoggingLevel(Log::VERBOSE);
    Log::setLogger(nullptr);

    // ---- GameMod ----
    CHECK(GameMod::values().size() == 13 && GameMod::values()[0] == &GameMod::EASY && GameMod::values()[12] == &GameMod::AUTO);
    CHECK(GameMod::valuesReversed()[0] == &GameMod::AUTO && GameMod::valuesReversed()[12] == &GameMod::EASY);
    CHECK(GameMod::HARD_ROCK.getKey() == input::Keys::A && input::Keys::A == 29 && input::Keys::Z == 54);
    CHECK(GameMod::AUTOPILOT.getBit() == 8192 && GameMod::AUTO.getAbbreviation().empty());
    GameMod::resetState();
    CHECK(GameMod::getModState() == 0 && GameMod::getModString(0) == "None");
    GameMod::EASY.toggle(true); GameMod::HARD_ROCK.toggle(true);          // mutually exclusive
    CHECK(!GameMod::EASY.isActive() && GameMod::HARD_ROCK.isActive());
    CHECK(near(GameMod::getDifficultyMultiplier(), 1.4f));
    GameMod::DOUBLE_TIME.toggle(true); GameMod::NIGHTCORE.toggle(true);   // DT/NC/HT exclusive
    CHECK(!GameMod::DOUBLE_TIME.isActive() && GameMod::NIGHTCORE.isActive());
    CHECK(near(GameMod::getSpeedMultiplier(), 1.5f));
    CHECK(GameMod::getModString(GameMod::getModState()) == "HardRock,Nightcore");
    GameMod::loadModState(64 | 512);                                       // DT|NC keeps only NC
    CHECK(!GameMod::DOUBLE_TIME.isActive() && GameMod::NIGHTCORE.isActive());
    GameMod::AUTO.toggle(true);
    GameMod::RELAX.toggle(true);                                           // can't combine with AUTO
    CHECK(!GameMod::RELAX.isActive());
    GameMod::resetState();
    CHECK(!GameMod::AUTO.isActive() && near(GameMod::getScoreMultiplier(), 1.f));

    // ---- PlaybackSpeed ----
    CHECK(getModifier(PlaybackSpeed::HALF) == 0.5f);
    CHECK(next(PlaybackSpeed::NORMAL) == PlaybackSpeed::DOUBLE);
    GameMod::DOUBLE_TIME.toggle(true);
    CHECK(next(PlaybackSpeed::NORMAL) == PlaybackSpeed::HALF);             // DOUBLE skipped under DT
    GameMod::resetState();

    // ---- TimingPoint ----
    TimingPoint tp("1000,500,4,2,1,70,0,1");
    CHECK(tp.getTime() == 1000 && near(tp.getBeatLength(), 500) && !tp.isInherited() && tp.isKiaiTimeActive());
    TimingPoint inh("0,-50,4,1,0,100,1,0");
    CHECK(inh.isInherited() && near(inh.getSliderMultiplier(), 0.5f));
    CHECK(near(TimingPoint("0,500,4,1,0,100,0,0").getSampleVolume(), 1.f));
    CHECK(throws<java::NumberFormatException>([] { TimingPoint t("0,abc,4,1,0,100"); }));
    CHECK(throws<std::out_of_range>([] { TimingPoint t("5"); }));
    CHECK(TimingPoint::DEFAULT.getBeatLength() == 1000.f);

    // ---- HitObject parsing ----
    HitObject c("256,148,9466,5,2,3:2:1:80:hit.wav");
    CHECK(c.isCircle() && c.isNewCombo() && c.getTime() == 9466 && c.getHitSoundType() == 2);
    CHECK(c.getHitSoundFile() == "hit.wav" && c.getHitSoundVolume() == 80 && c.getCustomSampleIndex() == 1);
    CHECK(c.getSampleSet(0) == 3 && c.getAdditionSampleSet(0) == 2);
    CHECK(c.toString() == "256,148,9466,5,2,3:2:1:80:hit.wav");
    HitObject sp("256,192,654,12,0,4029:7,0:0:0:0:");
    CHECK(sp.isSpinner() && sp.getEndTime() == 4029 && sp.getTypeName() == "spinner");
    CHECK(throws<java::NumberFormatException>([] { HitObject h("x,2,3,1,0"); }));
    CHECK(throws<std::out_of_range>([] { HitObject h("1,2,3"); }));
    CHECK(throws<java::NumberFormatException>([] { HitObject h("1,2,3,1,300"); }));   // byte overflow

    // ---- coordinate scaling ----
    HitObject::init(1920, 1080, 1.0f);   // 4:3 playfield centred in 16:9
    CHECK(near(HitObject::getXMultiplier(), 1440.f / 640.f) && near(HitObject::getYMultiplier(), 1080.f / 480.f));
    ReplayFrame rf(1, 2, 256, 192, ReplayFrame::KEY_K1);
    CHECK(rf.getScaledX() == static_cast<int>(256 * HitObject::getXMultiplier() + HitObject::getXOffset()));
    CHECK(rf.isKeyPressed() && ReplayFrame::KEY_K1 == 5 && ReplayFrame::getStartFrame(9).getY() == -500.f);
    HitObject::init(1920, 1080, 2.0f);   // zoomed playfield uses 512x384
    CHECK(near(HitObject::getXMultiplier(), 1440.f / 512.f));
    HitObject::init(1920, 1080, 1.0f);

    // ---- curves (640x480 => multiplier 1, so unscaled lengths mean what they say) ----
    HitObject::init(640, 480, 1.0f);
    CHECK(HitObject::getXMultiplier() == 1.f && HitObject::getXOffset() == 64 && HitObject::getYOffset() == 48);
    HitObject lin("100,100,500,2,0,L|200:100,1,100");
    auto lc = lin.getSliderCurve(false);
    CHECK(dynamic_cast<LinearBezier*>(lc.get()) != nullptr);
    CHECK(lc->getCurvePoints().size() == 100 / 5 + 1);
    CHECK(near(lc->getCurvePoints().front().x, 100, 0.01f) && near(lc->pointAt(0.5f).x, 150, 0.5f));
    CHECK(near(lc->getStartAngle(), 0, 0.01f) && near(std::fabs(lc->getEndAngle()), 180, 0.01f));
    CHECK(lc->indexAt(2.f) == static_cast<float>(lc->getCurvePoints().size() - 1) && lc->indexAt(-1.f) == 0.f);
    CHECK(throws<std::out_of_range>([&] { lc->getX(5); }));            // Java: ArrayIndexOutOfBounds

    HitObject arc("0,0,500,2,0,P|100:100|200:0,1,314.159");             // half circle, radius 100 (unscaled)
    auto ac = arc.getSliderCurve(false);
    CHECK(dynamic_cast<CircumscribedCircle*>(ac.get()) != nullptr);
    CHECK(near(ac->pointAt(0.5f).x, 100, 0.5f) && near(ac->pointAt(0.5f).y, 100, 0.5f));  // passes through the mid control point
    CHECK(near(ac->pointAt(1.f).x, 200, 0.5f) && near(ac->pointAt(1.f).y, 0, 0.5f));
    HitObject col("0,0,500,2,0,P|100:0|200:0,1,200");                   // collinear -> linear bezier fallback
    CHECK(dynamic_cast<LinearBezier*>(col.getSliderCurve(false).get()) != nullptr);
    HitObject cat("0,0,500,2,0,C|100:50|200:0|300:50,1,300");
    CHECK(dynamic_cast<CatmullCurve*>(cat.getSliderCurve(false).get()) != nullptr);
    CHECK(throws<std::runtime_error>([] {                               // 3 collinear points that aren't caught by getSliderCurve
        HitObject h("0,0,500,2,0,P|100:0|200:0,1,200");
        CircumscribedCircle cc(h, false);
    }));
    HitObject empty("0,0,500,2,0,L,1,100");                             // no control points, like Java: fails loudly
    CHECK(throws<std::exception>([&] { LinearBezier lb(empty, true, false); }));

    std::printf(failures ? "%d FAILED\n" : "batch2 tests passed\n", failures);
    return failures ? 1 : 0;
}
