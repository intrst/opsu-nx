// Tests for batch 14 (PerformanceCalculator). Expected numbers come from an independent Python re-implementation of the Java
// formulas (float32 where Java uses float, wrapped int32 where Java ints wrap); no JVM golden (the box has no javac).
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include "opsu/GameMod.h"
#include "opsu/beatmap/PerformanceCalculator.h"
#include "opsu/logging/StubLogger.h"
#include "opsu/logging/Log.h"

using namespace opsu;
using namespace opsu::PerformanceCalculator;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool near(double a, double b) { return std::fabs(a - b) <= 1e-9 * std::fmax(1.0, std::fabs(b)); }

struct Case {
    const char* name; double aimS, speedS; float od, ar; int nobj; int h300, h100, h50, miss, combo, maxCombo, mods;
    double e[10];   // aim speed acc reading total, then the same five for the perfect play
};

static const Case cases[] = {
    {"plain", 3.2, 2.7, 8.0, 9.0, 500, 480,15,3,2, 350,520, 0, {90.26954862153515, 50.9912239018527, 32.59953845763981, 16.060383703215468, 190.66541737721747, 133.62552149521787, 81.853585856, 66.06172862442514, 23.77409858060913, 305.6832952324445}},
    {"fc", 5.5, 5.0, 9.0, 9.5, 1200, 1200,0,0,0, 1200,1200, 0, {790.916623061465, 619.146838528577, 130.71407929041487, 31.11956465224138, 1610.7494423612566, 790.916623061465, 619.146838528577, 130.71407929041487, 31.11956465224138, 1610.7494423612566}},
    {"hr_hd", 4.1, 3.9, 7.0, 8.0, 800, 700,60,20,20, 500,900, 24, {116.17111929739991, 74.19769300814414, 15.585826654951859, 13.413546439805089, 223.52618988616638, 359.0926614591923, 276.6876407524, 174.89042391498236, 41.462164777333676, 856.5373434054142}},
    {"ez_nf_so", 2.0, 1.5, 6.0, 7.0, 300, 250,30,10,10, 120,310, 4099, {9.742967048368993, 2.642477100223945, 0.26115691731801255, 0.08433547116046797, 11.528572686306251, 30.15718840225845, 11.939881572802195, 6.947843033164207, 0.2610417011730762, 43.39673467322241}},
    {"fl_hd_highar", 6.0, 5.5, 9.5, 10.8, 2500, 2400,60,20,20, 1500,2600, 1032, {563.6951296369741, 318.35453241702, 91.57982984338962, 25.170243400707736, 1023.5991458905506, 1634.6547171328916, 982.6506696282432, 193.39457755031825, 72.99097498473607, 2965.1721466148847}},
    {"noar_unknown_max", 3.0, 3.0, 5.0, -1.0, -1, 90,8,1,1, 60,0, 0, {93.61137644736198, 75.50556686059619, 1.9418980291697712, 2.843099657793476, 181.15178625165495, 100.0932483910882, 99.12185421973479, 11.569957029864971, 3.039962567030065, 220.43594530144722}},
    {"longmap", 4.5, 4.2, 8.5, 9.2, 3000, 2900,70,20,10, 2000,3000, 0, {272.9175381768286, 215.73950111600774, 63.70293745860743, 18.18962643624389, 580.460951765087, 518.2363336292176, 435.8422373438006, 115.37489099172396, 34.539829786594716, 1124.0825688757395}},
    {"lowar", 1.0, 1.0, 4.0, 3.0, 100, 95,3,1,1, 70,100, 0, {2.366510013229126, 2.0820546560131565, 2.9758579071441744, 0.0, 7.533412086497925, 3.3087424500612017, 3.2419934322300175, 7.60365991066486, 0.0, 14.472225007217517}},
    {"zero_stars", 0.0, 0.0, 5.0, 5.0, 100, 100,0,0,0, 100,100, 0, {9.631809e-06, 9.538333333333333e-06, 11.569957029864971, 3.039962567030065, 15.640168279589892, 9.631809e-06, 9.538333333333333e-06, 11.569957029864971, 3.039962567030065, 15.640168279589892}},
    {"extra_hits_vs_objects", 3.0, 3.0, 8.0, 9.0, 100, 90,20,10,5, 50,100, 0, {44.89262664745183, 23.721182840052542, 0.02385026224975086, 9.726746333499515, 80.66289184482116, 101.88888330066173, 104.90150872840603, 40.76232359109008, 22.07594868199419, 270.4508387711315}},
    {"hr_clamp", 4.0, 3.5, 8.0, 9.0, 600, 550,30,10,10, 400,610, 16, {138.82114945638898, 87.43363126626905, 33.12684183106485, 16.28562793309632, 279.0452458131185, 272.554832546219, 192.79261665930318, 161.55575439788055, 31.974426170628494, 661.31565009946}},
    {"ar_10_4", 4.0, 3.5, 8.0, 10.4, 600, 550,30,10,10, 400,610, 0, {138.5857647169895, 78.22111571477922, 14.307439229034731, 17.99886384640938, 254.2911478813048, 272.09268936067286, 184.2504207211679, 69.77571692273315, 35.33811195837588, 567.3168299660763}},
};

static Beatmap makeMap(float od, float ar, int nobj) {
    Beatmap b("x.osu");
    b.overallDifficulty = od;
    b.approachRate = ar;
    if (nobj >= 0) {
        std::vector<HitObject> v;
        for (int i = 0; i < nobj; i++) v.emplace_back("256,192," + std::to_string(1000 + i * 100) + ",1,0");
        b.objects = std::move(v);
    }
    return b;
}

static void checkResult(const char* name, const Result& r, const double* e) {
    bool ok = near(r.aim, e[0]) && near(r.speed, e[1]) && near(r.accuracy, e[2]) && near(r.reading, e[3]) && near(r.total, e[4]) &&
              near(r.maxAim, e[5]) && near(r.maxSpeed, e[6]) && near(r.maxAccuracy, e[7]) && near(r.maxReading, e[8]) &&
              near(r.maxTotal, e[9]);
    if (!ok) { std::printf("FAIL case %s: total %.12g vs %.12g, maxTotal %.12g vs %.12g\n", name, r.total, e[4], r.maxTotal, e[9]); failures++; }
}

static bool allZero(const Result& r) {
    return r.aim == 0 && r.speed == 0 && r.accuracy == 0 && r.reading == 0 && r.total == 0 &&
           r.maxAim == 0 && r.maxSpeed == 0 && r.maxAccuracy == 0 && r.maxReading == 0 && r.maxTotal == 0;
}

int main() {
    Log::setLogger(&StubLogger::get());

    // every scenario vs the reference values (covers HR/EZ/HD/FL/NF/SO, AR bonus both ways, unknown AR/maxCombo/object count)
    for (const Case& c : cases) {
        Beatmap b = makeMap(c.od, c.ar, c.nobj);
        Result r = calculateFromStars(c.aimS, c.speedS, b, c.h300, c.h100, c.h50, c.miss, c.combo, c.maxCombo, c.mods);
        checkResult(c.name, r, c.e);
    }
    // the provider overload gives the same numbers and receives the beatmap
    {
        const Case& c = cases[0];
        Beatmap b = makeMap(c.od, c.ar, c.nobj);
        const Beatmap* seen = nullptr;
        Result r = calculate([&](const Beatmap& bm, double& a, double& s) { seen = &bm; a = c.aimS; s = c.speedS; },
                             b, c.h300, c.h100, c.h50, c.miss, c.combo, c.maxCombo, c.mods);
        CHECK(seen == &b);
        checkResult("provider", r, c.e);
    }
    // a throwing provider (std::exception or not) -> all zeroes, no throw
    {
        Beatmap b = makeMap(8.f, 9.f, 100);
        Result r1 = calculate([](const Beatmap&, double&, double&) { throw std::runtime_error("boom"); }, b, 90, 5, 3, 2, 50, 100, 0);
        CHECK(allZero(r1));
        Result r2 = calculate([](const Beatmap&, double&, double&) { throw 7; }, b, 90, 5, 3, 2, 50, 100, 0);
        CHECK(allZero(r2));
    }
    // early outs: negative stars (either one), no hits at all
    {
        Beatmap b = makeMap(8.f, 9.f, 100);
        CHECK(allZero(calculateFromStars(-1.0, 2.0, b, 90, 5, 3, 2, 50, 100, 0)));
        CHECK(allZero(calculateFromStars(2.0, -0.5, b, 90, 5, 3, 2, 50, 100, 0)));
        CHECK(allZero(calculateFromStars(2.0, 2.0, b, 0, 0, 0, 0, 0, 100, 0)));
        CHECK(!allZero(calculateFromStars(0.0, 0.0, b, 1, 0, 0, 0, 1, 1, 0)));   // 0 stars is allowed
        // int overflow of the hit total wraps negative in Java -> "no hits"
        CHECK(allZero(calculateFromStars(2.0, 2.0, b, 2147483647, 1, 0, 0, 5, 5, 0)));
    }
    // ratio(): 0 when max <= 0, clamped to [0,1]
    {
        CHECK(Result::ratio(5, 0) == 0.f && Result::ratio(5, -1) == 0.f);
        CHECK(Result::ratio(-3, 10) == 0.f && Result::ratio(30, 10) == 1.f);
        CHECK(near(Result::ratio(5, 10), 0.5));
        Result r;
        r.aim = 1; r.maxAim = 4; r.speed = 2; r.maxSpeed = 2; r.accuracy = 1; r.maxAccuracy = 0; r.reading = 3; r.maxReading = 6;
        CHECK(r.aimRatio() == 0.25f && r.speedRatio() == 1.f && r.accuracyRatio() == 0.f && r.readingRatio() == 0.5f);
    }
    // a perfect play reaches its own maximum
    {
        Beatmap b = makeMap(9.f, 9.5f, 1200);
        Result r = calculateFromStars(5.5, 5.0, b, 1200, 0, 0, 0, 1200, 1200, 0);
        CHECK(r.total == r.maxTotal && r.aimRatio() == 1.f && r.speedRatio() == 1.f);
    }

    Log::setLogger(nullptr);
    if (failures) { std::printf("batch14: %d FAILED\n", failures); return 1; }
    std::printf("batch14: all tests passed\n");
    return 0;
}
