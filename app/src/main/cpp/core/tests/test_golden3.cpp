// Third golden suite: regenerates the output of tests/golden/java/Golden3.java with the C++ port
// (Beatmap + BeatmapHPDropRateCalculator) and diffs it against tests/golden/expected3.txt, which was
// produced by the REAL Java classes (gen3.sh). Statement order mirrors the Java line for line: Java
// evaluates arguments left to right, C++ does not, so every rnd() call is sequenced explicitly.
// usage: opsu_golden3 [expected3.txt] [--dump]
#include <chrono>
#include <cmath>
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
#include "opsu/beatmap/BeatmapHPDropRateCalculator.h"
#include "opsu/logging/Log.h"
#include "opsu/util/Color.h"
#include "opsu/util/JavaCompat.h"

using namespace opsu;

static std::string out;
static uint64_t seed_ = 424242424242ULL;
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
static const char* B(bool b) { return b ? "true" : "false"; }
static std::string ex(const std::exception& e) {
    if (dynamic_cast<const java::NumberFormatException*>(&e)) return "NFE";
    if (dynamic_cast<const std::out_of_range*>(&e)) return "OOB";
    return "EX";
}
static Beatmap bm() { return Beatmap("x.osu"); }
static std::string nn(const std::optional<std::string>& s) { return s ? "'" + *s + "'" : "null"; }

// Counts Log::warn calls (the Java stub counts them the same way).
struct CountLogger : ILogger {
    int warns = 0;
    void setSaveToFile(bool) override {}
    void trace(const std::string&, const std::string&) override {}
    void trace(const std::string&, const std::string&, const std::exception&) override {}
    void debug(const std::string&, const std::string&) override {}
    void debug(const std::string&, const std::string&, const std::exception&) override {}
    void info(const std::string&, const std::string&) override {}
    void info(const std::string&, const std::string&, const std::exception&) override {}
    void warn(const std::string&, const std::string&) override { warns++; }
    void warn(const std::string&, const std::string&, const std::exception&) override { warns++; }
    void error(const std::string&, const std::string&) override {}
    void error(const std::string&, const std::string&, const std::exception&) override {}
};
static CountLogger counter;

// Options stub values (the Java stub has them as statics; C++ passes them explicitly).
static float optFixedSpeed = 0.f, optFixedCS = 0.f;
static bool optUnicode = false;
static const std::vector<Color> SKIN_COMBO = {Color(1, 2, 3), Color(250, 251, 252)};
static const Color SKIN_BORDER(9, 8, 7);

static Beatmap::ModParams currentModParams() { return Beatmap::ModParams::current(optFixedSpeed, optFixedCS); }

// ------------------------------------------------------------------ Color
static void colors() {
    for (int v = 0; v < 256; v++) {
        Color c(v, 255 - v, (v * 7) % 256);
        line("COLOR " + S(v) + " " + S(c.getRed()) + " " + S(c.getGreen()) + " " + S(c.getBlue()) + " " + hx(c.r) + " " + hx(c.g) + " " + hx(c.b) + " " + hx(c.a));
    }
    for (int i = 0; i < 64; i++) {
        int r = rnd(256); int g = rnd(256); int b = rnd(256); int a = rnd(256);
        Color c(r, g, b, a);
        Color d(c);
        line("COLORA " + S(r) + " " + S(g) + " " + S(b) + " " + S(a) + " " + S(c.getRed()) + " " + S(c.getGreen()) + " " + S(c.getBlue()) + " " + S(c.getAlpha()) + " " + hx(d.a));
    }
    // colours built from arbitrary floats: this is where truncation ((int)(r * 255)) differs from rounding
    for (int i = 0; i < 64; i++) {
        int ri = rnd(1001); int gi = rnd(1001); int bi = rnd(1001);
        Color c(ri / 1000.f, gi / 1000.f, bi / 1000.f);
        line("COLORF " + S(ri) + " " + S(gi) + " " + S(bi) + " " + S(c.getRed()) + " " + S(c.getGreen()) + " " + S(c.getBlue()) + " " + hx(c.a));
    }
}

// ------------------------------------------------------------------ breaks
static void breaks() {
    const std::vector<std::optional<std::string>> fixed = {
        std::nullopt, std::string(""), std::string("5"), std::string("1,2,3,4"), std::string("1,,2"), std::string(" 7"),
        std::string("1,2,"), std::string(","), std::string("a"), std::string("2147483648"), std::string("-5,6"),
        std::string("0"), std::string("00012,+3")};
    for (size_t i = 0; i < fixed.size(); i++) {
        Beatmap b = bm();
        std::string r;
        try { b.breaksFromString(fixed[i]); r = "ok"; } catch (const std::exception& t) { r = ex(t); }
        line("BRK " + S(i) + " " + r + " " + nn(b.breaks ? b.breaksToString() : std::nullopt));
    }
    for (int i = 0; i < 40; i++) {
        Beatmap b = bm();
        int n = rnd(9);
        b.breaks = std::vector<int>();
        for (int j = 0; j < n; j++) { int v = rnd(100000) - 20000; b.breaks->push_back(v); }
        std::optional<std::string> s = b.breaksToString();
        Beatmap b2 = bm();
        std::string r;
        try { b2.breaksFromString(s); r = "ok"; } catch (const std::exception& t) { r = ex(t); }
        line("BRKR " + S(i) + " " + nn(s) + " " + r + " " + nn(b2.breaks ? b2.breaksToString() : std::nullopt));
    }
    Beatmap n = bm();
    line("BRKNULL " + nn(n.breaksToString()));
}

// ------------------------------------------------------------------ timing points
static void tpLine(const std::string& tag, int i, const std::optional<std::string>& s) {
    Beatmap b = bm();
    int w0 = counter.warns;
    std::string r;
    try { b.timingPointsFromString(s); r = "ok"; } catch (const std::exception& t) { r = ex(t); }
    std::string sb;
    if (b.timingPoints)
        for (const TimingPoint& p : *b.timingPoints) sb += p.toString() + "#";
    line(tag + " " + S(i) + " " + r + " warns=" + S(counter.warns - w0) + " n=" + S(b.timingPoints ? static_cast<long long>(b.timingPoints->size()) : -1) + " " + sb + " " + nn(b.timingPointsToString()));
}
static void timing() {
    const std::vector<std::optional<std::string>> fixed = {
        std::nullopt, std::string(""), std::string("0,1000.0,4,1,0,100,0,0"),
        std::string("70382,-66.6666666666667,4,2,1,70,0,1|1000,500,4,1,0,100,1,0"), std::string("abc|1,2,3"), std::string("5"),
        std::string("1.5,300,4,1,0,100,0,1|"), std::string("|"),
        std::string("0,500,4,1,0,100,0,0|garbage|10,-100,3,2,1,50,0,0"), std::string("12,0,4,1,0,100,0,0"),
        std::string("7,250.25,6,3,2,80,1,1")};
    for (size_t i = 0; i < fixed.size(); i++) tpLine("TP", static_cast<int>(i), fixed[i]);
    for (int i = 0; i < 40; i++) {
        int k = 1 + rnd(4);
        std::string sb;
        for (int j = 0; j < k; j++) {
            int t = rnd(300000);
            int neg = rnd(2);
            int whole = 20 + rnd(700);
            int frac = rnd(1000);
            int meter = 2 + rnd(6);
            int st = rnd(4);
            int sc = rnd(3);
            int vol = rnd(101);
            int kiai = rnd(2);
            if (j > 0) sb += '|';
            sb += S(t) + "," + (neg == 0 ? "" : "-") + S(whole) + "." + S(frac) + "," + S(meter) + "," + S(st) + "," + S(sc) + "," + S(vol) + "," + S(neg) + "," + S(kiai);
        }
        tpLine("TPR", i, sb);
    }
}

// ------------------------------------------------------------------ combo / slider border
static std::string colorBits(const Color& c) { return hx(c.r) + "/" + hx(c.g) + "/" + hx(c.b) + "/" + hx(c.a); }
static void combo() {
    const std::vector<std::optional<std::string>> fixed = {
        std::nullopt, std::string("255,0,0|0,255,0"), std::string("1,2"), std::string(""), std::string("1,2,3|"),
        std::string("a,b,c"), std::string(" 1,2,3"), std::string("300,-5,7|1,2,3"), std::string("10,20,30|40,50"),
        std::string("1,2,3|a,b,c")};
    for (size_t i = 0; i < fixed.size(); i++) {
        Beatmap b = bm();
        b.combo = std::vector<Color>{Color(11, 22, 33)};
        std::string r;
        try { b.comboFromString(fixed[i]); r = "ok"; } catch (const std::exception& t) { r = ex(t); }
        line("CMB " + S(i) + " " + r + " " + S(b.combo->size()) + " " + nn(b.comboToString()) + " " + colorBits((*b.combo)[0]));
    }
    for (int i = 0; i < 60; i++) {
        int k = 1 + rnd(5);
        std::string sb;
        for (int j = 0; j < k; j++) {
            int r = rnd(256); int g = rnd(256); int bl = rnd(256);
            if (j > 0) sb += '|';
            sb += S(r) + "," + S(g) + "," + S(bl);
        }
        Beatmap b = bm();
        b.comboFromString(sb);
        line("CMBR " + S(i) + " " + sb + " " + *b.comboToString() + " " + colorBits((*b.combo)[0]) + " " + colorBits(b.combo->back()));
    }
    Beatmap nul = bm();
    line("CMBNULL " + nn(nul.comboToString()) + " " + nn(nul.sliderBorderToString()));
}
static void sliderBorder() {
    const std::vector<std::optional<std::string>> fixed = {
        std::nullopt, std::string("1,2,3"), std::string("1,2"), std::string("x"), std::string(""),
        std::string("255,255,255"), std::string("0,0,0"), std::string("300,-1,9")};
    for (size_t i = 0; i < fixed.size(); i++) {
        Beatmap b = bm();
        b.sliderBorder = Color(5, 6, 7);
        std::string r;
        try { b.sliderBorderFromString(fixed[i]); r = "ok"; } catch (const std::exception& t) { r = ex(t); }
        line("SB " + S(i) + " " + r + " " + nn(b.sliderBorderToString()) + " " + colorBits(*b.sliderBorder));
    }
    for (int i = 0; i < 40; i++) {
        int r = rnd(256); int g = rnd(256); int bl = rnd(256);
        Beatmap b = bm();
        b.sliderBorderFromString(S(r) + "," + S(g) + "," + S(bl));
        line("SBR " + S(i) + " " + S(r) + "," + S(g) + "," + S(bl) + " " + *b.sliderBorderToString() + " " + colorBits(*b.sliderBorder));
    }
}

// ------------------------------------------------------------------ ordering, text, accessors
static void order() {
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float ods[] = {0.f, -0.f, 5.f, 7.5f, NaN, 10.f, 5.f, -1.f};
    const int ODN = 8;
    const int cnt[] = {0, 1, 100, 2000000000, 2147483647, -5};
    const int CN = 6;
    for (int i = 0; i < 300; i++) {
        Beatmap a = bm(); Beatmap b = bm();
        a.overallDifficulty = ods[rnd(ODN)];
        b.overallDifficulty = ods[rnd(ODN)];
        a.hitObjectCircle = cnt[rnd(CN)]; a.hitObjectSlider = cnt[rnd(CN)]; a.hitObjectSpinner = cnt[rnd(CN)];
        b.hitObjectCircle = cnt[rnd(CN)]; b.hitObjectSlider = cnt[rnd(CN)]; b.hitObjectSpinner = cnt[rnd(CN)];
        line("CMP " + S(i) + " " + hx(a.overallDifficulty) + " " + hx(b.overallDifficulty) + " " + S(a.compareTo(b)) + " " + S(b.compareTo(a)) + " " + S(a.compareTo(a)));
    }
    const std::vector<std::string> ts = {"", "Title", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "t\xC3\xA9st"};
    const std::vector<std::string> srcs = {"", "Src"};
    for (int i = 0; i < 80; i++) {
        Beatmap b = bm();
        b.title = ts[rnd(4)]; b.titleUnicode = ts[rnd(4)];
        b.artist = ts[rnd(4)]; b.artistUnicode = ts[rnd(4)];
        b.source = srcs[rnd(2)];
        b.version = ts[rnd(4)];
        optUnicode = false;
        std::string s0 = b.toString(optUnicode), t0 = b.getTitle(optUnicode), a0 = b.getArtist(optUnicode);
        optUnicode = true;
        std::string s1 = b.toString(optUnicode), t1 = b.getTitle(optUnicode), a1 = b.getArtist(optUnicode);
        optUnicode = false;
        line("STR " + S(i) + " '" + s0 + "' '" + t0 + "' '" + a0 + "' '" + s1 + "' '" + t1 + "' '" + a1 + "'");
    }
    // skin fallbacks
    Beatmap b = bm();
    const std::vector<Color>* sc = &b.getComboColors(SKIN_COMBO);
    line("SKIN " + S(sc->size()) + " " + colorBits((*sc)[0]) + " " + colorBits((*sc)[1]) + " " + colorBits(b.getSliderBorderColor(SKIN_BORDER)));
    b.combo = std::vector<Color>{Color(12, 34, 56)};
    b.sliderBorder = Color(65, 43, 21);
    sc = &b.getComboColors(SKIN_COMBO);
    line("OWN " + S(sc->size()) + " " + colorBits((*sc)[0]) + " " + colorBits(b.getSliderBorderColor(SKIN_BORDER)));
    // play counter / additional fields
    Beatmap src = bm();
    src.starRating = 4.25; src.dateAdded = 1234567890123LL; src.favorite = true; src.playCount = 7; src.lastPlayed = 99; src.localMusicOffset = -12;
    Beatmap dst = bm();
    src.copyAdditionalFields(dst);
    line("COPY " + hx(dst.starRating) + " " + S(dst.dateAdded) + " " + B(dst.favorite) + " " + S(dst.playCount) + " " + S(dst.lastPlayed) + " " + S(dst.localMusicOffset));
    // the Java prints a check against System.currentTimeMillis(); the equivalent here
    int64_t before = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    dst.incrementPlayCounter();
    int64_t after = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    line("PLAY " + S(dst.playCount) + " " + B(dst.lastPlayed >= before && dst.lastPlayed <= after));
}

// ------------------------------------------------------------------ mod params / mod stars
static void mods() {
    const float fs[] = {0.f, 0.5f, 1.f, 1.25f, 1.5f, 2.f, -1.f};
    const float cs[] = {0.f, 4.2f, 7.f, 11.f, -2.f};
    const float bcs[] = {0.f, 2.5f, 4.f, 5.f, 7.3f, 9.9f, 10.f};
    for (int i = 0; i < 400; i++) {
        int st = rnd(1 << 13);
        GameMod::loadModState(st);
        optFixedSpeed = fs[rnd(7)];
        optFixedCS = cs[rnd(5)];
        Beatmap b = bm();
        b.circleSize = bcs[rnd(7)];
        int sr = rnd(1000);
        b.starRating = 1 + sr / 100.0;
        Beatmap::ModParams p = currentModParams();
        line("MOD " + S(i) + " " + S(st) + " " + S(p.key) + " " + hx(p.clockRate) + " " + hx(p.circleSize(b)) + " " + B(p.isNoMod(b)) + " " + B(b.needsModStars(p)) + " " + hx(b.getStarRating(currentModParams())));
        b.setModStars(p, 6.25, currentModParams());
        line("MOD2 " + S(i) + " " + B(b.needsModStars(p)) + " " + hx(b.getStarRating(currentModParams())));
        int st2 = rnd(1 << 13);
        GameMod::loadModState(st2);
        Beatmap::ModParams q = currentModParams();
        b.setModStars(p, 7.5, currentModParams());   // stale unless the key is unchanged
        line("MOD3 " + S(i) + " " + S(st2) + " " + B(p.key == q.key) + " " + B(b.needsModStars(q)) + " " + hx(b.getStarRating(currentModParams())));
        GameMod::loadModState(st);   // back to the first mods: was the stale result really dropped?
        line("MOD5 " + S(i) + " " + B(b.needsModStars(p)) + " " + hx(b.getStarRating(currentModParams())));
        GameMod::loadModState(st2);
        b.setModStars(q, 8.5, currentModParams());
        line("MOD4 " + S(i) + " " + B(b.needsModStars(q)) + " " + hx(b.getStarRating(currentModParams())));
    }
    GameMod::loadModState(0);
    optFixedSpeed = 0.f;
    optFixedCS = 0.f;
}

// ------------------------------------------------------------------ HP drop rate
static void hpCase(const std::string& tag, int t, const Beatmap& b, float hpd, float od) {
    try {
        BeatmapHPDropRateCalculator c(b, hpd, od);
        c.calculate();
        line(tag + " " + S(t) + " ok " + hx(c.getHpDropRate()) + " " + hx(c.getHpMultiplierNormal()) + " " + hx(c.getHpMultiplierComboEnd()));
    } catch (const std::exception& e) {
        line(tag + " " + S(t) + " " + ex(e));
    }
}
static void hp() {
    const float ticks[] = {1.f, 2.f, 0.5f, 3.f, 4.f};
    for (int t = 0; t < 150; t++) {
        int n = 5 + rnd(100);
        int time = 500 + rnd(4000);
        std::vector<HitObject> list;
        std::vector<int> br;
        for (int i = 0; i < n; i++) {
            int kind = rnd(10);
            int r4 = rnd(4);
            int nc = (r4 == 0 || i == 0) ? 4 : 0;
            int x = rnd(512); int y = rnd(384);
            std::string ln; int end;
            if (kind < 7) {
                ln = S(x) + "," + S(y) + "," + S(time) + "," + S(1 | nc) + ",0";
                end = time;
            } else if (kind < 9) {
                int rep = 1 + rnd(3); int len = 50 + rnd(350); int cx = rnd(512); int cy = rnd(384);
                ln = S(x) + "," + S(y) + "," + S(time) + "," + S(2 | nc) + ",0,B|" + S(cx) + ":" + S(cy) + "," + S(rep) + "," + S(len);
                end = time + rep * len * 3;
            } else {
                int dur = 800 + rnd(3000);
                ln = "256,192," + S(time) + "," + S(8 | nc) + ",0," + S(time + dur);
                end = time + dur;
            }
            list.emplace_back(ln);
            int g1 = 80 + rnd(900);
            int r12 = rnd(12);
            if (r12 == 0) {
                int big = 3000 + rnd(4000);
                br.push_back(end + 500); br.push_back(end + big - 500);
                g1 = big;
            }
            time = end + g1;
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
        b.HPDrainRate = rnd(101) / 10.f;
        b.overallDifficulty = rnd(101) / 10.f;
        b.sliderMultiplier = 1.f + rnd(26) / 10.f;
        b.sliderTickRate = ticks[rnd(5)];
        int rb = rnd(2);
        b.objects = list;
        b.timingPoints = tps;
        if (!(br.empty() && rb == 0)) b.breaks = br;
        float hpd = rnd(101) / 10.f;
        float od = rnd(101) / 10.f;
        hpCase("HP", t, b, hpd, od);
        line("HPIN " + S(t) + " " + S(n) + " " + S(tpn) + " " + S(br.size()) + " " + hx(hpd) + " " + hx(od) + " " + hx(b.sliderMultiplier) + " " + hx(b.sliderTickRate) + " " + hx(b.overallDifficulty));
    }
    // long runs of single-slider combos with high drain (see the comment in Golden3.java for why sliders)
    for (int t = 0; t < 60; t++) {
        int n = 40 + rnd(60);
        int gap = 150 + rnd(600);
        int time = 1000;
        std::vector<HitObject> list;
        for (int i = 0; i < n; i++) {
            int x = rnd(512); int y = rnd(384); int cx = rnd(512); int cy = rnd(384);
            int rep = 1 + rnd(2); int len = 200 + rnd(150);
            list.emplace_back(S(x) + "," + S(y) + "," + S(time) + ",6,0,B|" + S(cx) + ":" + S(cy) + "," + S(rep) + "," + S(len));
            time += rep * len * 4 + gap;
        }
        Beatmap b = bm();
        b.objects = list;
        b.timingPoints = std::vector<TimingPoint>();
        b.timingPoints->emplace_back("0,500.0,4,1,0,100,0,0");
        int hd = rnd(21);
        int od = rnd(101);
        float hpd = 8.f + hd / 10.f;
        float odf = od / 10.f;
        hpCase("HPC", t, b, hpd, odf);
        line("HPCIN " + S(t) + " " + S(n) + " " + S(gap) + " " + hx(hpd) + " " + hx(odf));
    }
    // error handling, in the order the real code fails
    Beatmap e1 = bm();                                            // objects null
    e1.timingPoints = std::vector<TimingPoint>();
    hpCase("HPERR", 1, e1, 5.f, 5.f);
    Beatmap e2 = bm();                                            // objects empty, timing points null
    e2.objects = std::vector<HitObject>();
    hpCase("HPERR", 2, e2, 5.f, 5.f);
    Beatmap e3 = bm();                                            // timing points null
    e3.objects = std::vector<HitObject>{HitObject("1,2,1000,1,0"), HitObject("1,2,2000,1,0")};
    hpCase("HPERR", 3, e3, 5.f, 5.f);
    Beatmap e4 = bm();                                            // odd break list
    e4.objects = std::vector<HitObject>{HitObject("1,2,1000,5,0"), HitObject("1,2,2000,1,0"), HitObject("1,2,3000,5,0")};
    e4.timingPoints = std::vector<TimingPoint>();
    e4.timingPoints->emplace_back("0,500,4,1,0,100,0,0");
    e4.breaks = std::vector<int>{100000000, 200000000, 5};
    hpCase("HPERR", 4, e4, 5.f, 5.f);
    Beatmap e5 = bm();                                            // no timing points at all
    e5.objects = std::vector<HitObject>{HitObject("1,2,1000,5,0"), HitObject("1,2,2000,1,0"), HitObject("1,2,3000,5,0")};
    e5.timingPoints = std::vector<TimingPoint>();
    hpCase("HPERR", 5, e5, 5.f, 5.f);
}

int main(int argc, char** argv) {
    std::string path = "tests/golden/expected3.txt";
    bool dump = false;
    for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--dump")) dump = true; else path = argv[i]; }
    Log::setLogger(&counter);
    colors(); breaks(); timing(); combo(); sliderBorder(); order(); mods(); hp();
    if (dump) { std::cout << out; return 0; }

    std::ifstream f(path);
    if (!f) { std::printf("cannot open %s\n", path.c_str()); return 2; }
    std::stringstream expSs; expSs << f.rdbuf();
    std::istringstream a(expSs.str()), b(out);
    std::string la, lb;
    int n = 0, bad = 0;
    while (true) {
        bool ga = static_cast<bool>(std::getline(a, la)), gb = static_cast<bool>(std::getline(b, lb));
        if (!ga && !gb) break;
        n++;
        if (!ga || !gb || la != lb) {
            if (bad++ < 12) {
                auto cut = [](const std::string& s) { return s.size() > 300 ? s.substr(0, 300) + "..." : s; };
                std::printf("MISMATCH line %d\n  java: %s\n  c++ : %s\n", n, cut(ga ? la : "<eof>").c_str(), cut(gb ? lb : "<eof>").c_str());
            }
        }
    }
    std::printf(bad ? "golden3: %d of %d lines differ\n" : "golden3: all %d lines match\n", bad ? bad : n, n);
    return bad ? 1 : 0;
}
