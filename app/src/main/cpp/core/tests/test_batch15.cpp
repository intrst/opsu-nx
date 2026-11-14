// Batch 15: BeatmapDifficultyCalculator, the PerformanceCalculator::calculate(beatmap, ...) overload, and the fix for
// negative-length curves (was a segfault). Numbers vs the real Java: see test_golden4.cpp.
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "opsu/beatmap/Beatmap.h"
#include "opsu/beatmap/BeatmapDifficultyCalculator.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/beatmap/PerformanceCalculator.h"
#include "opsu/beatmap/TimingPoint.h"
#include "opsu/logging/ILogger.h"
#include "opsu/logging/Log.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

template <class E, class F>
static bool throws(F f) { try { f(); } catch (const E&) { return true; } catch (...) { return false; } return false; }

struct CaptureLogger : ILogger {
    int errors = 0, warns = 0; std::string lastError, lastWarn;
    void setSaveToFile(bool) override {}
    void trace(const std::string&, const std::string&) override {}
    void trace(const std::string&, const std::string&, const std::exception&) override {}
    void debug(const std::string&, const std::string&) override {}
    void debug(const std::string&, const std::string&, const std::exception&) override {}
    void info(const std::string&, const std::string&) override {}
    void info(const std::string&, const std::string&, const std::exception&) override {}
    void warn(const std::string&, const std::string& m) override { warns++; lastWarn = m; }
    void warn(const std::string&, const std::string& m, const std::exception&) override { warns++; lastWarn = m; }
    void error(const std::string&, const std::string& m) override { errors++; lastError = m; }
    void error(const std::string&, const std::string& m, const std::exception&) override { errors++; lastError = m; }
};

static Beatmap makeMap(const std::vector<std::string>& objs, const std::vector<std::string>& tps) {
    Beatmap b("x.osu");
    std::vector<HitObject> o; for (const auto& l : objs) o.emplace_back(l);
    std::vector<TimingPoint> t; for (const auto& l : tps) t.emplace_back(l);
    b.objects = std::move(o); b.timingPoints = std::move(t);
    return b;
}
static std::string circle(int x, int y, int t) { return std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(t) + ",1,0"; }
static double stars(const Beatmap& b, float cs, float clock) {
    BeatmapDifficultyCalculator c(b, cs, clock); c.calculate(); return c.getStarRating();
}

int main() {
    CaptureLogger log; Log::setLogger(&log);
    const std::vector<std::string> TP = {"0,500,4,1,0,100,0,0"};

    // ---- negative-length curves: used to dereference an empty vector (segfault); Java throws ----
    HitObject::init(640, 480, 1.0f);
    for (const char* len : {"-4", "-5", "-9"}) {   // ncurve = 0 / -1: Java gets a 1- or 0-element array, then indexes [0]
        HitObject h(std::string("100,100,300,2,0,L|150:150,1,") + len);
        if (std::string(len) == "-4") CHECK(h.getSliderCurve(false)->getCurvePoints().size() == 1);
        else CHECK(throws<std::out_of_range>([&] { h.getSliderCurve(false); }));          // ArrayIndexOutOfBounds
    }
    for (const char* len : {"-10", "-50", "-1e9"}) {  // ncurve + 1 < 0: NegativeArraySizeException
        HitObject h(std::string("100,100,300,2,0,L|150:150,1,") + len);
        CHECK(throws<java::NegativeArraySizeException>([&] { h.getSliderCurve(false); }));
    }
    {   // perfect-circle curves: (int) step + 1 elements. -7 => step -1.4 => 0 elements (fine), -60 => negative size
        HitObject ok("0,0,500,2,0,P|100:100|200:0,1,-7");
        CHECK(ok.getSliderCurve(false)->getCurvePoints().empty());
        HitObject neg("0,0,500,2,0,P|100:100|200:0,1,-60");
        CHECK(throws<java::NegativeArraySizeException>([&] { neg.getSliderCurve(false); }));
    }

    // ---- BeatmapDifficultyCalculator: unloaded / empty beatmaps ----
    {
        Beatmap b("x.osu");                                      // nothing loaded
        BeatmapDifficultyCalculator c(b);
        c.calculate();
        CHECK(log.errors == 1 && c.getStarRating() == -1 && c.getStars(0) == -1 && c.getDifficulty(1) == -1);
        b.objects = std::vector<HitObject>{HitObject(circle(10, 10, 100))};   // timing points still missing
        BeatmapDifficultyCalculator c2(b); c2.calculate();
        CHECK(log.errors == 2 && c2.getStarRating() == -1);
        Beatmap e = makeMap({}, TP);   // empty map: a warning, then "Could not compute strain values" as an error; values stay -1
        BeatmapDifficultyCalculator c3(e); c3.calculate();
        CHECK(log.errors == 3 && log.warns == 1 && c3.getStarRating() == -1);
        CHECK(log.lastWarn.find("empty beatmap") != std::string::npos && log.lastError.find("strain values") != std::string::npos);
        CHECK(throws<std::out_of_range>([&] { c3.getStars(2); }) && throws<std::out_of_range>([&] { c3.getDifficulty(-1); }));
        // errors are logged with the beatmap's description, and calculating again just logs again
        c.calculate(); CHECK(log.errors == 4);
    }

    // ---- clock rate / circle size parameters ----
    HitObject::init(1920, 1080, 1.0f);
    std::vector<std::string> o;
    for (int i = 0; i < 40; i++) o.push_back(circle((i % 2) ? 450 : 60, (i % 3) * 120 + 40, 500 + i * 170));
    Beatmap m = makeMap(o, TP);
    double base = stars(m, m.circleSize, 1.f);
    CHECK(base > 0 && std::isfinite(base));
    CHECK(stars(m, m.circleSize, 0.f) == base && stars(m, m.circleSize, -2.f) == base);   // clockRate <= 0 means 1
    CHECK(stars(m, m.circleSize, std::numeric_limits<float>::quiet_NaN()) == base);       // NaN too
    CHECK(BeatmapDifficultyCalculator(m).getStarRating() == -1);                          // not calculated yet
    {   BeatmapDifficultyCalculator c(m); c.calculate(); CHECK(c.getStarRating() == base); }
    CHECK(stars(m, m.circleSize, 1.5f) > base);          // DoubleTime: less time between objects => harder
    CHECK(stars(m, m.circleSize, 0.75f) < base);         // HalfTime
    CHECK(stars(m, 2.f, 1.f) < stars(m, 7.f, 1.f));      // bigger circles (low CS) are easier: same pixel jumps count for less

    // ---- sanity: dense long jumps are harder than a calm pattern ----
    std::vector<std::string> calm, hard;
    for (int i = 0; i < 30; i++) { calm.push_back(circle(250 + (i % 2) * 20, 190, 400 + i * 600)); hard.push_back(circle((i % 2) ? 480 : 30, (i % 2) ? 350 : 30, 400 + i * 130)); }
    CHECK(stars(makeMap(hard, TP), 5.f, 1.f) > 3 * stars(makeMap(calm, TP), 5.f, 1.f));

    // ---- characterization of Java behaviour that the port keeps (see PORTING_NOTES.md "Findings") ----
    {   // (a) the strain of the last interval is never added to highestStrains: objects that all fall into the first
        //     400 ms interval produce difficulty 0 however far apart they are
        Beatmap two = makeMap({circle(10, 10, 100), circle(500, 380, 300)}, TP);
        BeatmapDifficultyCalculator c(two); c.calculate();
        CHECK(c.getDifficulty(0) == 0 && c.getDifficulty(1) == 0 && c.getStarRating() == 0);
        Beatmap three = makeMap({circle(10, 10, 100), circle(500, 380, 300), circle(10, 10, 900)}, TP);
        BeatmapDifficultyCalculator c3(three); c3.calculate();
        CHECK(c3.getDifficulty(1) > 0);
    }
    {   // (b) slider curves are built unscaled but their length is multiplied by HitObject::getXMultiplier(), so the
        //     result depends on the last HitObject::init()
        Beatmap sl = makeMap({circle(10, 10, 100), "100,100,1000,2,0,L|400:100,2,50", circle(300, 300, 1500), circle(50, 50, 2500)},
                             {"0,500,4,1,0,100,0,0"});
        HitObject::init(640, 480, 1.0f);   double small = stars(sl, 5.f, 1.f);
        HitObject::init(1920, 1080, 1.0f); double big = stars(sl, 5.f, 1.f);
        CHECK(small != big);
    }

    // ---- PerformanceCalculator: the three entry points agree ----
    {
        HitObject::init(1920, 1080, 1.0f);
        Beatmap pm = makeMap(o, TP);
        pm.overallDifficulty = 8.f; pm.approachRate = 9.f;
        BeatmapDifficultyCalculator c(pm); c.calculate();
        auto direct = PerformanceCalculator::calculateFromStars(c.getStars(BeatmapDifficultyCalculator::DIFFICULTY_AIM),
                                                                c.getStars(BeatmapDifficultyCalculator::DIFFICULTY_SPEED), pm, 35, 4, 1, 0, 30, 40, 0);
        auto conv = PerformanceCalculator::calculate(pm, 35, 4, 1, 0, 30, 40, 0);
        auto viaProvider = PerformanceCalculator::calculate(
            [](const Beatmap& b, double& a, double& s) { BeatmapDifficultyCalculator d(b); d.calculate(); a = d.getStars(1); s = d.getStars(0); },
            pm, 35, 4, 1, 0, 30, 40, 0);
        CHECK(direct.total > 0 && conv.total == direct.total && viaProvider.total == direct.total && conv.maxTotal == direct.maxTotal);
        CHECK(conv.aim == direct.aim && conv.speed == direct.speed && conv.reading == direct.reading);
        // an unloaded beatmap gives all zeroes (and an error log from the difficulty calculator)
        int before = log.errors;
        Beatmap un("x.osu");
        auto z = PerformanceCalculator::calculate(un, 10, 0, 0, 0, 10, 10, 0);
        CHECK(z.total == 0 && z.maxTotal == 0 && log.errors > before);
        // a slider whose curve cannot be built is caught like the Java `catch (Exception)`
        Beatmap bad = makeMap({circle(10, 10, 100), "100,100,300,2,0,B,1,100"}, TP);
        before = log.errors;
        auto zb = PerformanceCalculator::calculate(bad, 10, 0, 0, 0, 10, 10, 0);
        CHECK(zb.total == 0 && log.errors == before + 1);
    }

    Log::setLogger(nullptr);
    std::printf(failures ? "%d FAILED\n" : "batch15: all tests passed\n", failures);
    return failures ? 1 : 0;
}
