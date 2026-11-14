// Fourth golden suite: regenerates the output of tests/golden/java/Golden4.java with the C++ port
// (BeatmapDifficultyCalculator + PerformanceCalculator) and diffs it against tests/golden/expected4.txt,
// which was produced by the REAL Java classes (gen4.sh). Statement order mirrors the Java line for line: Java
// evaluates arguments left to right, C++ does not, so every rnd() call is sequenced explicitly.
// usage: opsu_golden4 [expected4.txt] [--dump] [--strict]
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include "opsu/GameMod.h"
#include "opsu/beatmap/Beatmap.h"
#include "opsu/beatmap/BeatmapDifficultyCalculator.h"
#include "opsu/beatmap/PerformanceCalculator.h"
#include "opsu/logging/Log.h"
#include "opsu/util/JavaCompat.h"

using namespace opsu;

static std::string out;
static uint64_t seed_ = 777000777ULL;
static int rnd() { seed_ = seed_ * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<int>(seed_ >> 33); }
static int rnd(int n) { return rnd() % n; }
static std::string hx(float f) {
    uint32_t u;
    if (f != f) u = 0x7fc00000u; else std::memcpy(&u, &f, 4);
    char b[16]; std::snprintf(b, sizeof b, "%08x", u); return b;
}
static std::string hx(double d) {
    uint64_t u;
    if (d != d) u = 0x7ff8000000000000ull; else std::memcpy(&u, &d, 8);
    char b[24]; std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u)); return b;
}
static void line(const std::string& s) { out += s; out += '\n'; }
static std::string S(long long v) { return std::to_string(v); }
static std::string ex(const std::exception& e) {
    if (dynamic_cast<const java::NumberFormatException*>(&e)) return "NFE";
    if (dynamic_cast<const std::out_of_range*>(&e)) return "OOB";
    return "EX";
}
static Beatmap bm() { return Beatmap("x.osu"); }

// Counts Log::error / Log::warn calls (the Java stub counts them the same way).
struct CountLogger : ILogger {
    int errors = 0, warns = 0;
    void setSaveToFile(bool) override {}
    void trace(const std::string&, const std::string&) override {}
    void trace(const std::string&, const std::string&, const std::exception&) override {}
    void debug(const std::string&, const std::string&) override {}
    void debug(const std::string&, const std::string&, const std::exception&) override {}
    void info(const std::string&, const std::string&) override {}
    void info(const std::string&, const std::string&, const std::exception&) override {}
    void warn(const std::string&, const std::string&) override { warns++; }
    void warn(const std::string&, const std::string&, const std::exception&) override { warns++; }
    void error(const std::string&, const std::string&) override { errors++; }
    void error(const std::string&, const std::string&, const std::exception&) override { errors++; }
};
static CountLogger counter;
static std::string logs() { return "E" + S(counter.errors) + " W" + S(counter.warns); }

static const int CFG[6][3] = {{640, 480, 10}, {1920, 1080, 10}, {1280, 720, 16}, {800, 600, 10}, {1080, 2400, 10}, {512, 384, 13}};
static void initCfg(const int* c) { HitObject::init(c[0], c[1], static_cast<float>(c[2]) / 10.f); }

/** kind: 0 mixed, 1 streams (dense circles), 2 slider heavy. */
static Beatmap genMap(int kind) {
    int n = 2 + rnd(kind == 2 ? 40 : 70);
    int time = 200 + rnd(3000);
    int sliderPct = (kind == 2) ? 70 : (kind == 1 ? 5 : 30);
    int spinnerPct = (kind == 1) ? 0 : 6;
    const char* types = "BLPC";
    std::vector<HitObject> list;
    for (int i = 0; i < n; i++) {
        int k = rnd(100);
        int x = rnd(513); int y = rnd(385);
        std::string ln; int end;
        if (k < sliderPct) {
            char t = types[rnd(4)];
            int pts = (t == 'P') ? 2 : 1 + rnd(4);
            int rep = 1 + rnd(4);
            int len = 20 + rnd(480); int dec = rnd(10);
            std::string sb;
            sb += S(x) + "," + S(y) + "," + S(time) + ",2,0,"; sb += t;
            for (int p = 0; p < pts; p++) { int cx = rnd(513); int cy = rnd(385); sb += "|" + S(cx) + ":" + S(cy); }
            sb += "," + S(rep) + "," + S(len) + "." + S(dec);
            ln = sb;
            end = time + rep * len * 2;
        } else if (k < sliderPct + spinnerPct) {
            int dur = 500 + rnd(2500);
            ln = "256,192," + S(time) + ",8,0," + S(time + dur);
            end = time + dur;
        } else {
            ln = S(x) + "," + S(y) + "," + S(time) + ",1,0";
            end = time;
        }
        list.emplace_back(ln);
        int gap;
        if (kind == 1) gap = 40 + rnd(120);
        else if (rnd(10) == 0) gap = rnd(60);
        else gap = 100 + rnd(900);
        time = end + gap;
    }
    int tpn = 1 + rnd(6);
    std::vector<TimingPoint> tps;
    int bw = 300 + rnd(500); int bf = rnd(10);
    tps.emplace_back("0," + S(bw) + "." + S(bf) + ",4,1,0,100,0,0");
    int tt = 0;
    for (int j = 1; j < tpn; j++) {
        tt += 2000 + rnd(15000);
        int kindTp = rnd(2);
        if (kindTp == 0) {
            int w2 = 300 + rnd(500); int f2 = rnd(10);
            tps.emplace_back(S(tt) + "," + S(w2) + "." + S(f2) + ",4,1,0,100,0,0");
        } else {
            int v = 25 + rnd(200);
            tps.emplace_back(S(tt) + ",-" + S(v) + ".5,4,1,0,100,0,0");
        }
    }
    Beatmap b = bm();
    b.circleSize = static_cast<float>(rnd(101)) / 10.f;
    b.sliderMultiplier = 1.f + static_cast<float>(rnd(26)) / 10.f;
    b.overallDifficulty = static_cast<float>(rnd(101)) / 10.f;
    int arSel = rnd(3);
    int arVal = rnd(101);
    b.approachRate = (arSel == 0) ? -1.f : static_cast<float>(arVal) / 10.f;
    b.objects = std::move(list);
    b.timingPoints = std::move(tps);
    return b;
}

static void diffCase(const std::string& tag, int t, const Beatmap& b, bool two, float cs, float clock) {
    try {
        std::unique_ptr<BeatmapDifficultyCalculator> c;
        if (two) c = std::make_unique<BeatmapDifficultyCalculator>(b, cs, clock);
        else c = std::make_unique<BeatmapDifficultyCalculator>(b);
        c->calculate();
        line(tag + " " + S(t) + " ok " + hx(c->getStarRating()) + " " + hx(c->getDifficulty(0)) + " " + hx(c->getDifficulty(1)) + " " +
             hx(c->getStars(0)) + " " + hx(c->getStars(1)) + " " + logs());
    } catch (const std::exception& e) {
        line(tag + " " + S(t) + " " + ex(e) + " " + logs());
    }
}

static Beatmap withTps(std::vector<HitObject> objs, const std::vector<std::string>& tpLines) {
    Beatmap b = bm();
    b.objects = std::move(objs);
    std::vector<TimingPoint> tps;
    for (const auto& l : tpLines) tps.emplace_back(l);
    b.timingPoints = std::move(tps);
    return b;
}

static void diff() {
    const float css[] = {0.f, 2.5f, 4.f, 5.f, 7.f, 9.5f, 10.f, 12.142858f, -3.f};
    const float clocks[] = {1.f, 1.5f, 0.75f, 0.f, -1.f, 2.f, 1.3333334f, std::numeric_limits<float>::quiet_NaN()};
    // before any HitObject::init(): the static multipliers are still 0
    for (int t = 0; t < 6; t++) {
        Beatmap b = genMap(t % 3);
        diffCase("DIFF0", t, b, false, 0.f, 0.f);
    }
    for (int t = 0; t < 180; t++) {
        int kind = t % 3;
        const int* cfg = CFG[rnd(6)];
        initCfg(cfg);
        Beatmap b = genMap(kind);
        line("DIFFIN " + S(t) + " " + S(static_cast<long long>(b.objects->size())) + " " + S(static_cast<long long>(b.timingPoints->size())) + " " +
             S(cfg[0]) + "x" + S(cfg[1]) + "@" + S(cfg[2]) + " " + hx(b.circleSize) + " " + hx(b.sliderMultiplier));
        diffCase("DIFF", t, b, false, 0.f, 0.f);
        int ci = rnd(9); int ki = rnd(8);
        diffCase("DIFF2", t, b, true, css[ci], clocks[ki]);
    }
    // getDifficulty / getStars with a bad type
    initCfg(CFG[1]);
    Beatmap g = genMap(0);
    try {
        BeatmapDifficultyCalculator c(g);
        c.calculate();
        std::string r, r2;
        try { c.getStars(2); r = "noex"; } catch (const std::exception& e) { r = ex(e); }
        try { c.getDifficulty(-1); r2 = "noex"; } catch (const std::exception& e) { r2 = ex(e); }
        line("DIFFTYPE " + r + " " + r2);
    } catch (const std::exception& e) { line("DIFFTYPE " + ex(e)); }

    // error / edge cases
    Beatmap e1 = bm(); e1.timingPoints = std::vector<TimingPoint>{TimingPoint("0,500,4,1,0,100,0,0")};
    diffCase("DIFFERR", 1, e1, false, 0.f, 0.f);                      // objects not loaded
    Beatmap e2 = bm(); e2.objects = std::vector<HitObject>{HitObject("10,10,100,1,0")};
    diffCase("DIFFERR", 2, e2, false, 0.f, 0.f);                      // timing points not loaded
    Beatmap e3 = bm(); e3.objects = std::vector<HitObject>{}; e3.timingPoints = std::vector<TimingPoint>{};
    diffCase("DIFFERR", 3, e3, false, 0.f, 0.f);                      // empty map
    Beatmap e4 = withTps({HitObject("10,10,100,1,0")}, {});
    diffCase("DIFFERR", 4, e4, false, 0.f, 0.f);                      // single object, no timing points
    Beatmap e5 = withTps({HitObject("10,10,100,1,0"), HitObject("200,100,100,1,0"), HitObject("300,300,100,1,0")}, {"0,-100,4,1,0,100,1,0"});
    diffCase("DIFFERR", 5, e5, false, 0.f, 0.f);                      // all at the same time, first timing point inherited
    Beatmap e6 = withTps({HitObject("10,10,100,1,0"), HitObject("100,100,300,2,0,B,1,100")}, {"0,500,4,1,0,100,0,0"});
    diffCase("DIFFERR", 6, e6, false, 0.f, 0.f);                      // slider without control points: curve construction fails
    Beatmap e7 = withTps({HitObject("10,10,100,1,0"), HitObject("100,100,300,2,0,L|150:150,1,0")}, {"0,500,4,1,0,100,0,0"});
    diffCase("DIFFERR", 7, e7, false, 0.f, 0.f);                      // zero-length slider
    Beatmap e8 = withTps({HitObject("10,10,100,1,0"), HitObject("100,100,300,2,0,L|150:150,1,-50")}, {"0,500,4,1,0,100,0,0"});
    diffCase("DIFFERR", 8, e8, false, 0.f, 0.f);                      // negative slider length
    Beatmap e9 = withTps({HitObject("10,10,100,2,0,B|60:60|90:20,2,150.5"), HitObject("200,100,1000,2,0,L|250:100,3,120"), HitObject("300,300,2000,1,0")},
                         {"0,-50,4,1,0,100,1,0", "100,400,4,1,0,100,0,0", "1000,-200,4,1,0,100,1,0", "9000,300,4,1,0,100,0,0"});
    diffCase("DIFFERR", 9, e9, false, 0.f, 0.f);

    // timing-point boundary (see Golden4.java)
    initCfg(CFG[1]);
    for (int v = 0; v < 4; v++) {
        int inhTime = (v % 2 == 0) ? 1000 : 1001;
        int rep = (v < 2) ? 2 : 3;
        Beatmap e = withTps({HitObject("10,10,100,1,0"), HitObject("100,100,1000,2,0,L|400:100," + S(rep) + ",50"),
                             HitObject("300,300,1500,1,0"), HitObject("50,50,2500,1,0")},
                            {"0,10,4,1,0,100,0,0", S(inhTime) + ",-1000,4,1,0,100,1,0"});
        e.sliderMultiplier = 3.5f;
        diffCase("DIFFTP", v, e, false, 0.f, 0.f);
    }
}

static void perf() {
    for (int t = 0; t < 160; t++) {
        int kind = t % 3;
        const int* cfg = CFG[rnd(6)];
        initCfg(cfg);
        Beatmap b = genMap(kind);
        int h300 = rnd(3000); int h100 = rnd(300); int h50 = rnd(60); int miss = rnd(40);
        int zero = rnd(15);
        if (zero == 0) { h300 = 0; h100 = 0; h50 = 0; miss = 0; }
        int combo = rnd(h300 + h100 + h50 + miss + 1);
        int mcSel = rnd(4); int mcAdd = rnd(200);
        int maxCombo = (mcSel == 0) ? 0 : combo + mcAdd;
        int mods = rnd(16384);
        PerformanceCalculator::Result r = PerformanceCalculator::calculate(b, h300, h100, h50, miss, combo, maxCombo, mods);
        line("PERF " + S(t) + " " + S(static_cast<long long>(b.objects->size())) + " " + S(h300) + " " + S(h100) + " " + S(h50) + " " + S(miss) + " " +
             S(combo) + " " + S(maxCombo) + " " + S(mods) + " " + hx(b.overallDifficulty) + " " + hx(b.approachRate) + " | " +
             hx(r.aim) + " " + hx(r.speed) + " " + hx(r.accuracy) + " " + hx(r.reading) + " " + hx(r.total) + " " +
             hx(r.maxAim) + " " + hx(r.maxSpeed) + " " + hx(r.maxAccuracy) + " " + hx(r.maxReading) + " " + hx(r.maxTotal) + " " +
             hx(r.aimRatio()) + " " + hx(r.speedRatio()) + " " + hx(r.accuracyRatio()) + " " + hx(r.readingRatio()) + " " + logs());
    }
    // ratio() corner cases
    const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
    const double rc[10][2] = {{1, 2}, {3, 2}, {-1, 2}, {0, 0}, {5, -1}, {nan, 1}, {1, nan}, {inf, 1}, {0, 1e-300}, {0.5, 1}};
    for (int i = 0; i < 10; i++) line("RATIO " + S(i) + " " + hx(PerformanceCalculator::Result::ratio(rc[i][0], rc[i][1])));
    // error / edge cases
    Beatmap e1 = bm(); e1.timingPoints = std::vector<TimingPoint>{TimingPoint("0,500,4,1,0,100,0,0")};
    PerformanceCalculator::Result r1 = PerformanceCalculator::calculate(e1, 100, 0, 0, 0, 100, 100, 0);
    line("PERFERR 1 " + hx(r1.total) + " " + hx(r1.maxTotal) + " " + logs());   // objects not loaded
    Beatmap e2 = withTps({HitObject("10,10,100,1,0"), HitObject("100,100,300,2,0,B,1,100")}, {"0,500,4,1,0,100,0,0"});
    PerformanceCalculator::Result r2 = PerformanceCalculator::calculate(e2, 100, 0, 0, 0, 100, 100, 0);
    line("PERFERR 2 " + hx(r2.total) + " " + hx(r2.maxTotal) + " " + logs());   // curve failure is caught
    Beatmap e3 = genMap(0);
    PerformanceCalculator::Result r3 = PerformanceCalculator::calculate(e3, 0, 0, 0, 0, 0, 0, 0);
    line("PERFERR 3 " + hx(r3.total) + " " + hx(r3.maxTotal) + " " + logs());   // no hits at all
    Beatmap e4 = genMap(0);
    PerformanceCalculator::Result r4 = PerformanceCalculator::calculate(e4, std::numeric_limits<int>::max(), 5, 0, 0, 1, 1, 0);
    line("PERFERR 4 " + hx(r4.total) + " " + hx(r4.maxTotal) + " " + logs());   // totalHits overflows to a negative int
}

// Math.pow differs between JVMs/libms in the last bit for ~0.1% of calls (HotSpot intrinsic vs glibc vs
// fdlibm vs Android bionic), and no implementation is systematically right. Everything that is pure
// arithmetic is compared bit-for-bit; the double-valued results that go through pow (DIFF* values
// after "ok", PERF values after '|') may differ by a few ulps. A real logic bug (e.g. float-vs-double
// narrowing) is ~1e8 ulp, so it is still caught. `--strict` demands bit-exactness.
static const int TOL_ULP = 16;
static bool isHex(const std::string& t, size_t len) {
    if (t.size() != len) return false;
    for (char c : t) if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    return true;
}
static std::vector<std::string> tokens(const std::string& l) {
    std::vector<std::string> v; std::istringstream is(l); std::string t;
    while (is >> t) v.push_back(t);
    return v;
}
static bool sameWithinLibmTolerance(const std::string& a, const std::string& b) {
    std::vector<std::string> x = tokens(a), y = tokens(b);
    if (x.size() != y.size() || x.empty() || x[0] != y[0]) return false;
    size_t from;
    if (x[0].rfind("DIFF", 0) == 0) { if (x.size() < 3 || x[2] != "ok" || y[2] != "ok") return false; from = 3; }  // DIFF, DIFF0, DIFF2, DIFFERR
    else if (x[0] == "PERF") { from = 0; while (from < x.size() && x[from] != "|") from++; if (from == x.size() || y[from] != "|") return false; from++; }
    else return false;
    for (size_t i = 0; i < x.size(); i++) {
        if (x[i] == y[i]) continue;
        if (i < from || !isHex(x[i], 16) || !isHex(y[i], 16)) {
            if (i >= from && isHex(x[i], 8) && isHex(y[i], 8)) {            // float ratios derived from the doubles: 1 ulp
                long long p = std::stoll(x[i], nullptr, 16), q = std::stoll(y[i], nullptr, 16);
                if ((p >> 31) == (q >> 31) && std::llabs(p - q) <= 1) continue;
            }
            return false;
        }
        unsigned long long p = std::stoull(x[i], nullptr, 16), q = std::stoull(y[i], nullptr, 16);
        if ((p >> 63) != (q >> 63)) return false;
        unsigned long long d = p > q ? p - q : q - p;
        if (d > static_cast<unsigned long long>(TOL_ULP)) return false;
    }
    return true;
}

int main(int argc, char** argv) {
    std::string path = "tests/golden/expected4.txt";
    bool dump = false, strict = false;
    for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--dump")) dump = true; else if (!std::strcmp(argv[i], "--strict")) strict = true; else path = argv[i]; }
    Log::setLogger(&counter);
    diff(); perf();
    if (dump) { std::cout << out; return 0; }

    std::ifstream f(path);
    if (!f) { std::printf("cannot open %s\n", path.c_str()); return 2; }
    std::stringstream expSs; expSs << f.rdbuf();
    std::istringstream a(expSs.str()), b(out);
    std::string la, lb;
    int n = 0, bad = 0, tolerated = 0;
    while (true) {
        bool ga = static_cast<bool>(std::getline(a, la)), gb = static_cast<bool>(std::getline(b, lb));
        if (!ga && !gb) break;
        n++;
        bool same = ga && gb && la == lb;
        if (!same && ga && gb && !strict && sameWithinLibmTolerance(la, lb)) { same = true; tolerated++; }
        if (!same) {
            if (bad++ < 12) {
                auto cut = [](const std::string& s) { return s.size() > 300 ? s.substr(0, 300) + "..." : s; };
                std::printf("MISMATCH line %d\n  java: %s\n  c++ : %s\n", n, cut(ga ? la : "<eof>").c_str(), cut(gb ? lb : "<eof>").c_str());
            }
        }
    }
    if (bad) std::printf("golden4: %d of %d lines differ\n", bad, n);
    else std::printf("golden4: all %d lines match (%d only within %d ulp: libm pow rounding, see tests/golden/README.md)\n", n, tolerated, TOL_ULP);
    return bad ? 1 : 0;
}
