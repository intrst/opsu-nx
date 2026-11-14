// Second golden suite: regenerates the output of tests/golden/java/Golden2.java with the C++ port
// and diffs it against tests/golden/expected2.txt (produced by the REAL Java classes).
// Statement order mirrors the Java line for line: Java evaluates arguments left to right, C++ does not,
// so every rnd() call is sequenced explicitly.
// usage: opsu_golden2 [expected2.txt] [--dump]
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "opsu/GameData.h"
#include "opsu/GameMod.h"
#include "opsu/Utils.h"
#include "opsu/beatmap/Health.h"
#include "opsu/io/OsuReader.h"
#include "opsu/io/OsuWriter.h"
#include "opsu/ui/KineticScrolling.h"
#include "opsu/ui/animations/AnimatedValue.h"
#include "opsu/ui/animations/AnimationEquation.h"
#include "opsu/ui/animations/AnimationTrack.h"
#include "opsu/ui/animations/EasedSlide.h"
#include "opsu/ui/animations/LazerAnimations.h"
#include "opsu/user/User.h"
#include "opsu/user/UserStats.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;
namespace L = opsu::LazerAnimations;
using AE = AnimationEquation;

static std::string out;
static uint64_t seed_ = 987654321ULL;
static int rnd() { seed_ = seed_ * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<int>(seed_ >> 33); }
static int rnd(int n) { return rnd() % n; }
// Canonical NaN, like Float.floatToIntBits / Double.doubleToLongBits.
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
static std::string bytesHex(const std::vector<uint8_t>& b) {
    std::string s; char t[4];
    for (uint8_t x : b) { std::snprintf(t, sizeof t, "%02x", x); s += t; }
    return s;
}
static std::string u8(const std::string& s) { return bytesHex(std::vector<uint8_t>(s.begin(), s.end())); }
static AE EQ(int i) { return static_cast<AE>(i); }
static const int NEQ = ANIMATION_EQUATION_COUNT;

// ------------------------------------------------------------------ easing
static void ease() {
    static const char* names[] = {"LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD", "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC", "IN_QUART",
        "OUT_QUART", "IN_OUT_QUART", "IN_QUINT", "OUT_QUINT", "IN_OUT_QUINT", "IN_SINE", "OUT_SINE", "IN_OUT_SINE", "IN_EXPO", "OUT_EXPO",
        "IN_OUT_EXPO", "IN_CIRC", "OUT_CIRC", "IN_OUT_CIRC", "IN_BACK", "OUT_BACK", "IN_OUT_BACK", "IN_BOUNCE", "OUT_BOUNCE", "IN_OUT_BOUNCE",
        "IN_ELASTIC", "OUT_ELASTIC", "OUT_ELASTIC_HALF", "OUT_ELASTIC_QUARTER", "IN_OUT_ELASTIC"};
    for (int i = 0; i < NEQ; i++) line("ENAME " + S(i) + " " + names[i]);
    std::vector<float> ts;
    float fixed[] = {0.f, 1.f, 0.5f, 0.25f, 0.75f, 1e-6f, 0.99999994f, 0.36363636f, 0.72727273f, 0.90909091f, 0.45f, 0.55f, -0.0f};
    for (float f : fixed) ts.push_back(f);
    for (int i = -16; i <= 80; i++) ts.push_back(static_cast<float>(i) / 64.f);
    for (int i = 0; i < 150; i++) ts.push_back(static_cast<float>(rnd(2000) - 500) / 1000.f);
    for (int i = 0; i < NEQ; i++)
        for (float t : ts) line("E " + S(i) + " " + hx(t) + " " + hx(calc(EQ(i), t)));
}

// ------------------------------------------------------------------ AnimatedValue / EasedSlide
static void animValue() {
    for (int round = 0; round < 40; round++) {
        int dur = 1 + rnd(2000);
        float min = static_cast<float>(rnd(2000) - 1000) / 10.f;
        float max = static_cast<float>(rnd(2000) - 1000) / 10.f;
        AnimatedValue v(dur, min, max, EQ(rnd(NEQ)));
        std::string s = "AV " + S(round) + " " + S(dur) + ":";
        for (int step = 0; step < 30; step++) {
            switch (rnd(6)) {
                case 0: v.setTime(rnd(dur * 2) - 100); break;
                case 1: v.setDuration(1 + rnd(2500)); break;
                case 2: v.setEquation(EQ(rnd(NEQ))); break;
                default: s += v.update(rnd(300) - 40) ? "T" : "F"; break;
            }
            s += " " + S(v.getTime()) + "/" + S(v.getDuration()) + "/" + hx(v.getValue()) + (v.isFinished() ? "!" : "");
        }
        line(s);
    }
}

static void slide() {
    for (int round = 0; round < 30; round++) {
        int a = rnd(600) - 50;
        int b = rnd(600) - 50;
        EasedSlide s(a, b, EQ(rnd(NEQ)));
        std::string o = "SL " + S(round) + ":";
        for (int step = 0; step < 40; step++) {
            switch (rnd(8)) {
                case 0: s.open(); break;
                case 1: s.close(); break;
                case 2: s.snap(rnd(2) == 0); break;
                default: o += s.update(rnd(120)) ? "T" : "F"; break;
            }
            o += " " + hx(s.get()) + (s.isOpening() ? 'o' : 'c') + (s.isVisible() ? 'v' : 'h') + (s.isIdle() ? 'i' : 'm');
        }
        line(o);
    }
}

// ------------------------------------------------------------------ AnimationTrack / LazerAnimations
static void track(const std::string& name, const AnimationTrack& t) {
    line("TD " + name + " " + hx(t.getDuration()) + " " + hx(t.getFinalValue()));
    for (float x = -10.f; x < 2000.f; x += 13.7f) line("TV " + name + " " + hx(x) + " " + hx(t.valueAt(x)));
    for (float x : {0.f, 96.f, 120.f, 120.00001f, 1100.f, 1400.f}) line("TV " + name + " " + hx(x) + " " + hx(t.valueAt(x)));
}

static void lazer() {
    track("JA", L::JUDGEMENT_ALPHA);
    track("JH", L::JUDGEMENT_HIT_SCALE);
    track("JMS", L::JUDGEMENT_MISS_SCALE);
    track("JMY", L::JUDGEMENT_MISS_Y);
    track("JMR", L::JUDGEMENT_MISS_ROTATION);
    track("LS", L::LIGHTING_SCALE);
    track("LA", L::LIGHTING_ALPHA);
    AnimationTrack custom(2.f);
    custom.to(5.f, -30.f, AE::OUT_BOUNCE).delay(-1.f).to(-3.f, 50.f, AE::IN_OUT_ELASTIC).set(7.f).to(1.f, 0.f, AE::LINEAR);
    for (int i = 0; i < 9; i++) custom.to(static_cast<float>(i), 10.f + static_cast<float>(i), EQ(i * 3));
    track("CU", custom);
    for (int i = 0; i < 60; i++) {
        float t = static_cast<float>(rnd(3000) - 500) / 7.f;
        float st = static_cast<float>(rnd(1000) - 200) / 3.f;
        float ms = (rnd(5) == 0) ? 0.f : static_cast<float>(rnd(900)) / 3.f;
        int e = rnd(NEQ);
        line("TP " + hx(t) + " " + hx(st) + " " + hx(ms) + " " + S(e) + " " + hx(AnimationTrack::progress(t, st, ms, EQ(e))));
    }
    line("LC " + hx(L::SPINNER_SPIN_Y) + " " + hx(L::SPINNER_CLEAR_Y) + " " + hx(L::SPINNER_BONUS_Y) + " " + hx(L::SPINNER_GLOW_R) + " " +
         hx(L::SPINNER_GLOW_G) + " " + hx(L::SPINNER_GLOW_B) + " " + S(L::FOLLOW_INITIAL_TIME) + " " + S(L::SPINNER_SPM_WINDOW) + " " +
         S(L::SPINNER_METRE_BARS));
    for (int i = 0; i < 400; i++) {
        int ar = rnd(2500) - 100;
        int td = rnd(3000) - 800;
        float t = static_cast<float>(rnd(4000) - 500) / 5.f;
        float d = static_cast<float>(rnd(2000) - 200) / 3.f;
        float prog = static_cast<float>(rnd(1300) - 100) / 1000.f;
        float base = static_cast<float>(rnd(1000)) / 1000.f;
        float tgt = static_cast<float>(rnd(2000) - 1000) / 10.f;
        float cur = static_cast<float>(rnd(2000) - 1000) / 10.f;
        std::string s = "LZ " + S(L::fadeInTime(ar)) + " " + hx(L::approachScale(td, ar)) + " " + hx(L::approachAlpha(td, ar, L::fadeInTime(ar))) + " ";
        int r1 = rnd(500) - 50;
        s += hx(L::approachAlpha(td, ar, r1)) + " " + hx(L::hitCircleProgress(t)) + " " + hx(L::hitCircleAlpha(t)) + " " +
             hx(L::followEndScale(t)) + " " + hx(L::followEndAlpha(t)) + " " + hx(L::reverseArrowScale(t, d)) + " " +
             hx(L::kiaiFlashAlpha(t, d)) + " " + hx(L::damp(cur, tgt, base, d)) + " " + hx(L::spinnerAlpha(t, d, static_cast<float>(ar))) + " " +
             hx(L::spinnerApproachScale(t, d)) + " ";
        bool nb = rnd(2) == 0;
        bool comp = rnd(9) == 0;
        float rf = static_cast<float>(rnd(1000)) / 1000.f;
        s += S(L::spinnerMetreBars(prog, nb, comp, rf)) + " " + hx(L::spinnerNewStyleScale(prog)) + " " + hx(L::spinnerSpmHidden(t, d)) + " " +
             hx(L::outQuad(t, d));
        line(s);
    }
}

// ------------------------------------------------------------------ KineticScrolling
static void kinetic() {
    for (int round = 0; round < 8; round++) {
        KineticScrolling k;
        k.setAllowOverScroll(round % 2 == 1);
        if (round >= 2) k.setMinMax(-500.f, 1500.f);
        if (round >= 4) k.setSpeedMultiplier(2.5f);
        std::string b = "K " + S(round) + ":";
        for (int step = 0; step < 250; step++) {
            switch (rnd(12)) {
                case 0: k.pressed(); break;
                case 1: k.released(); break;
                case 2: case 3: case 4: k.dragged(static_cast<float>(rnd(200) - 100) * 0.37f); break;
                case 5: k.scrollToPosition(static_cast<float>(rnd(3000) - 1000) * 1.f); break;
                case 6: k.scrollOffset(static_cast<float>(rnd(400) - 200) / 3.f); break;
                case 7: k.setPosition(static_cast<float>(rnd(2000) - 500)); break;
                case 8: k.addOffset(static_cast<float>(rnd(100)) / 4.f); break;
                default: k.update(static_cast<float>(rnd(90) + 1)); break;
            }
            b += " " + hx(k.getPosition()) + "/" + hx(k.getTargetPosition()) + (k.isPressed() ? 'p' : 'r');
        }
        line(b);
    }
    KineticScrolling k;
    line("KD " + hx(k.getMin()) + " " + hx(k.getMax()) + " " + hx(k.getSpeedMultiplier()));
    for (float m : {0.f, -1.f, 0.5f}) {
        try { k.setSpeedMultiplier(m); line("KM " + hx(m) + " ok"); }
        catch (const std::invalid_argument& e) { line("KM " + hx(m) + " IAE " + e.what()); }
    }
    for (float p : {0.5f, 1.5f, 2.5f, -0.5f, -1.5f, -2.5f, 0.49999997f}) {
        KineticScrolling r;
        r.setPosition(p);
        r.pressed(); r.released();
        line("KR " + hx(p) + " " + hx(r.getTargetPosition()));
    }
}

// ------------------------------------------------------------------ User / UserStats / grades
static std::string rep(const std::string& s, int from, int to) { return s.substr(from, to - from); }

static void user() {
    for (int l = -2; l <= 125; l++) line("UL " + S(l) + " " + S(User::getScoreForLevel(l)));
    line("UL 1000 " + S(User::getScoreForLevel(1000)));
    int64_t scores[] = {0, 1, 2, 4999, 5000, 123456789LL, 26931190828LL, 26931190829LL, 26931190830LL, 126931190829LL, 5000000000000LL,
                        INT64_MAX / 4};
    for (int64_t s : scores) {
        User u("n", s, 98.5, 3, 4, 1);
        line("UU " + S(s) + " " + S(u.getLevel()) + " " + hx(u.getNextLevelProgress()));
    }
    for (int i = 0; i < 60; i++) {
        int64_t a = rnd();
        int64_t bb = rnd(100000);
        int64_t c = rnd();
        int64_t s = a * bb + c;
        double acc = rnd(10000) / 100.0;
        int pp = rnd(50);
        int pt = rnd(80);
        User u("x", s, acc, pp, pt, 3);
        line("UR " + S(s) + " " + S(u.getLevel()) + " " + hx(u.getNextLevelProgress()));
    }
    User u("Player", 2);
    std::string b = "UA";
    for (int i = 0; i < 80; i++) {
        if (rnd(3) == 0) {
            u.add(static_cast<int64_t>(rnd(900000)));
        } else {
            int64_t sc = static_cast<int64_t>(rnd(2000000)) * 3;
            double acc = rnd(10001) / 100.0;
            u.add(sc, acc);
        }
        b += " " + S(u.getScore()) + "/" + hx(u.getAccuracy()) + "/" + S(u.getPassedPlays()) + "/" + S(u.getTotalPlays()) + "/" + S(u.getLevel()) + "/" +
             hx(u.getNextLevelProgress());
    }
    line(b);
    std::string av = "av.png", bpng = "b.png";
    User c("C", 5LL, 1.0, 1, 1, 7, 3, nullptr, &av, nullptr);
    line("UC " + S(c.getBanner()) + "|" + c.getBannerFile() + "|" + c.getAvatarFile() + "|" + c.getBio() + "|" + S(c.getIconId()));
    c.setBannerFile(&bpng); line("UC " + S(c.getBanner()) + "|" + c.getBannerFile());
    c.setBannerFile(nullptr); line("UC " + S(c.getBanner()) + "|" + c.getBannerFile());
    c.setBanner(4); line("UC " + S(c.getBanner()) + "|" + c.getBannerFile());
    c.setAvatarFile(nullptr); line("UC [" + c.getAvatarFile() + "]");
    std::string longBio;
    for (int i = 0; i < 100; i++) longBio += static_cast<char>('a' + i % 26);
    std::vector<const std::string*> bios;
    std::string e0 = "", e1 = "short", e2 = longBio, e3 = rep(longBio, 0, 90), e4 = rep(longBio, 0, 89),
                e5 = std::string("\xC3\xA9\xC3\xA9\xC3\xA9") + rep(longBio, 0, 95),
                e6 = std::string("\xE4\xB8\xAD\xE6\x96\x87") + rep(longBio, 0, 95),
                e7 = std::string("x\xF0\x9F\x98\x80y") + rep(longBio, 0, 95);
    bios = {nullptr, &e0, &e1, &e2, &e3, &e4, &e5, &e6, &e7};
    for (const std::string* s : bios) { c.setBio(s); line("UB " + S(java::utf16Length(c.getBio())) + " " + u8(c.getBio())); }
    const char* names[] = {"alice", "Alice", "ALICE", "bob", "Bob", "alicE2", "al", "", "Zed", "zed", "[x]", "_y", "a b"};
    for (const char* x : names)
        for (const char* y : names) {
            int cmp = User(x, 1).compareTo(User(y, 1));
            line("UCMP " + S(cmp > 0 ? 1 : (cmp < 0 ? -1 : 0)));
        }
}

static void grades() {
    line("GD " + S(GameData::HIT_MISS) + " " + S(GameData::HIT_300) + " " + S(GameData::HIT_MU) + " " + S(GameData::HIT_MAX));
    int cnt[] = {0, 1, 2, 3, 5, 9, 10, 19, 50, 99, 100, 101, 333, 1000};
    for (int a : cnt) for (int b : {0, 1, 7, 50}) for (int c : {0, 1, 3}) for (int d : {0, 1, 2})
        line("GR " + S(a) + " " + S(b) + " " + S(c) + " " + S(d) + " " + hx(GameData::getScorePercent(a, b, c, d)) + " " +
             GameData::gradeName(GameData::getGrade(a, b, c, d, false)) + " " + GameData::gradeName(GameData::getGrade(a, b, c, d, true)));
    for (int i = 0; i < 300; i++) {
        int a = rnd(500), b = rnd(60), c = rnd(20), d = rnd(15) / 8;
        bool silver = rnd(2) == 0;
        line("GRR " + S(a) + " " + S(b) + " " + S(c) + " " + S(d) + " " + hx(GameData::getScorePercent(a, b, c, d)) + " " +
             GameData::gradeName(GameData::getGrade(a, b, c, d, silver)));
    }
}

static void stats() {
    int modBits[] = {0, GameMod::HIDDEN.getBit(), GameMod::FLASHLIGHT.getBit(), GameMod::HARD_ROCK.getBit(),
                     GameMod::HIDDEN.getBit() | GameMod::FLASHLIGHT.getBit()};
    for (int round = 0; round < 12; round++) {
        UserStats s;
        int n = (round == 0) ? 0 : (round * 9) % 70;
        for (int i = 0; i < n; i++) {
            UserStats::Play p;
            if (rnd(5) == 0) p.timestamp = 0; else p.timestamp = 1600000000LL + rnd(10000000);
            p.title = "t" + std::to_string(i); p.artist = "a"; p.version = "v";
            p.hit300 = rnd(300); p.hit100 = rnd(40); p.hit50 = rnd(10);
            if (rnd(4) == 0) p.miss = rnd(5); else p.miss = 0;
            if (rnd(4) == 0) p.score = 100000LL; else p.score = static_cast<int64_t>(rnd(3000)) * 1000;
            p.combo = rnd(900); p.mods = modBits[rnd(5)]; p.perfect = rnd(3) == 0;
            if (rnd(6) == 0) { p.hit100 = p.hit50 = p.miss = 0; }
            s.add(p);
        }
        s.finish();
        std::string b = "US " + S(round) + " " + S(s.plays) + " " + S(s.hit300) + " " + S(s.hit100) + " " + S(s.hit50) + " " + S(s.miss) + " " + S(s.bestScore) +
                        " " + S(s.bestCombo) + " " + S(s.fullCombos) + " " + S(s.gradeSS) + " " + S(s.gradeS) + " " + S(s.gradeA) + " " + S(s.gradeB) + " " +
                        S(s.gradeC) + " " + S(s.gradeD) + " " + S(s.firstPlayed) + " " + S(s.lastPlayed) + " " + hx(s.averageAccuracy) + " " +
                        S(s.getTotalHits()) + " :";
        for (const auto& p : s.topPlays) b += " " + p.title + "/" + S(p.score) + "/" + hx(p.accuracy) + "/" + GameData::gradeName(p.grade);
        line(b);
    }
}

// ------------------------------------------------------------------ Health
static void health() {
    using namespace GameData;
    int hits[] = {HIT_MISS, HIT_50, HIT_100, HIT_300, HIT_100K, HIT_300K, HIT_300G, HIT_SLIDER10, HIT_SLIDER30, HIT_MAX, HIT_SLIDER_REPEAT,
                  HIT_SPINNERSPIN, HIT_SPINNERBONUS, HIT_MU};
    for (int round = 0; round < 16; round++) {
        Health h;
        h.setModifiers(static_cast<float>(round) * 0.7f, 1.f + static_cast<float>(round) * 0.03f, 1.f + static_cast<float>(round) * 0.07f);
        std::string b = "HP " + S(round) + ":";
        for (int step = 0; step < 120; step++) {
            switch (rnd(9)) {
                case 0: h.update(rnd(100)); break;
                case 1: h.setHealth(static_cast<float>(rnd(260) - 30)); break;
                case 2: h.changeHealth(static_cast<float>(rnd(400) - 200) / 3.f); break;
                case 3: if (rnd(10) == 0) h.reset(); break;
                default: h.changeHealthForHit(hits[rnd(14)]); break;
            }
            b += " " + hx(h.getRawHealth()) + "/" + hx(h.getUncappedRawHealth()) + "/" + hx(h.getHealth()) + "/" + hx(h.getHealthDisplay());
        }
        line(b);
    }
}

// ------------------------------------------------------------------ Utils
static void utils() {
    for (int i = 0; i < 200; i++) {
        float a = static_cast<float>(rnd(20000) - 10000) / 7.f;
        float b = static_cast<float>(rnd(20000) - 10000) / 7.f;
        float c = static_cast<float>(rnd(20000) - 10000) / 7.f;
        float d = static_cast<float>(rnd(20000) - 10000) / 7.f;
        float t = static_cast<float>(rnd(1000)) / 1000.f;
        line("UD " + hx(Utils::distance(a, b, c, d)) + " " + hx(Utils::lerp(a, b, t)));
    }
    for (int i = -3; i <= 53; i++) {
        float d = static_cast<float>(i) / 5.f;
        line("MD " + hx(d) + " " + hx(Utils::mapDifficultyRange(d, 195, 160, 60)) + " " + hx(Utils::mapDifficultyRange(d, -6, -25, -40)) + " " +
             hx(Utils::mapDifficultyRange(d, 1800, 1200, 450)));
    }
    for (int round = 0; round < 25; round++) {
        std::vector<int> l;
        int n = 1 + rnd(60);
        for (int i = 0; i < n; i++) l.push_back(rnd(400) - 200);
        line("SD " + S(n) + " " + hx(Utils::standardDeviation(l)));
    }
    line("SD0 " + hx(Utils::standardDeviation({})));
    for (int s : {-5, 0, 1, 2, 59, 60, 61, 599, 3599, 3600, 3661, 86399, 100000, 360000}) line("TS " + S(s) + " " + Utils::getTimeString(s));
    const int64_t byteCases[] = {0, 1, 1023, 1024, 1025, 1536, 1048575, 1048576, 123456789, 1073741824, 5368709120LL, 1099511627776LL,
                                 1125899906842624LL, 1152921504606846976LL, INT64_MAX};
    for (int64_t x : byteCases) line("BY " + S(x) + " " + Utils::bytesToString(x));
    for (int i = 0; i < 200; i++) {
        int64_t a = rnd();
        int sh = rnd(30);
        int64_t c = rnd();
        int64_t x = 1024LL + (a << sh) + c;
        if (x < 0) x = -x;
        line("BYR " + S(x) + " " + Utils::bytesToString(x));
    }
    const char* names[] = {"normal.osu", "a<b>c", "he said \"hi\"", "p|q", "col:on", "q?m*s", "back\\slash/fwd", "tab\there",
                           "\xC3\xA9\xE4\xB8\xAD\xE6\x96\x87.osz", "ctrl\x01\x1f" "x", "", "  sp  "};
    for (const char* s : names)
        line("CF " + u8(s) + " " + u8(Utils::cleanFileName(s, 0)) + " " + u8(Utils::cleanFileName(s, '_')) + " " + u8(Utils::cleanFileName(s, '*')) + " " +
             u8(Utils::cleanFileName(s, '-')));
}

// ------------------------------------------------------------------ OsuReader / OsuWriter
static std::string tryRead(const std::string& bytes, int what) {
    try {
        std::istringstream is(bytes);
        OsuReader r(is);
        switch (what) {
            case 0: return "i=" + S(r.readInt());
            case 1: return "s=" + S(r.readShort());
            case 2: return "l=" + S(r.readLong());
            case 3: return "u=" + S(r.readULEB128());
            case 4: return "str=" + u8(r.readString());
            default: return "d=" + S(r.readDate());
        }
    } catch (const java::EOFException&) {
        return "EOF";
    } catch (const java::IOException& e) {
        return std::string("IO:") + e.what();
    } catch (const std::exception&) {
        return "RT";
    }
}

static void io() {
    std::ostringstream bos;
    OsuWriter w(bos);
    w.write(static_cast<int8_t>(-5)); w.write(static_cast<int8_t>(127));
    w.write(static_cast<int16_t>(-2)); w.write(static_cast<int16_t>(0x1234));
    w.write(static_cast<int32_t>(0)); w.write(static_cast<int32_t>(-1)); w.write(static_cast<int32_t>(0x01020304)); w.write(static_cast<int32_t>(INT32_MIN));
    w.write(static_cast<int64_t>(0)); w.write(static_cast<int64_t>(-2)); w.write(static_cast<int64_t>(0x0102030405060708LL)); w.write(static_cast<int64_t>(INT64_MIN));
    w.write(1.5f); w.write(-0.f); w.write(std::nanf("")); w.write(3.4028235E38f);
    w.write(3.141592653589793); w.write(-0.0); w.write(-HUGE_VAL);
    w.write(true); w.write(false);
    for (int v : {0, 1, 127, 128, 300, 16383, 16384, 2097151, 268435455, 0x7FFFFFFF}) w.writeULEB128(v);
    w.write(std::string("")); w.write(std::string("")); w.write(std::string("hello")); w.write(std::string("Zenith (TV Size)"));
    std::string big;
    for (int i = 0; i < 200; i++) big += static_cast<char>('A' + i % 26);
    w.write(big);
    w.writeDate(0LL); w.writeDate(1600000000123LL); w.writeDate(-86400000LL); w.writeDate(253402300799999LL);
    w.write(std::vector<uint8_t>{1, 2, 0xFF});
    w.write(std::string("caf\xC3\xA9")); w.write(std::string("\xE4\xB8\xAD\xE6\x96\x87" "abc")); w.write(std::string("x\xF0\x9F\x98\x80y"));
    w.close();
    std::string s = bos.str();
    std::vector<uint8_t> bytes(s.begin(), s.end());
    line("OW " + S(static_cast<long long>(bytes.size())) + " " + bytesHex(bytes));

    std::istringstream is(s);
    OsuReader r(is);
    {   // each Java line concatenates in order; sequence reads explicitly
        auto b0 = r.readByte(); auto b1 = r.readByte(); auto s0 = r.readShort(); auto s1 = r.readShort();
        line("OR " + S(b0) + " " + S(b1) + " " + S(s0) + " " + S(s1));
        auto i0 = r.readInt(); auto i1 = r.readInt(); auto i2 = r.readInt(); auto i3 = r.readInt();
        line("OR " + S(i0) + " " + S(i1) + " " + S(i2) + " " + S(i3));
        auto l0 = r.readLong(); auto l1 = r.readLong(); auto l2 = r.readLong(); auto l3 = r.readLong();
        line("OR " + S(l0) + " " + S(l1) + " " + S(l2) + " " + S(l3));
        auto f0 = r.readSingle(); auto f1 = r.readSingle(); auto f2 = r.readSingle(); auto f3 = r.readSingle();
        line("OR " + hx(f0) + " " + hx(f1) + " " + hx(f2) + " " + hx(f3));
        auto d0 = r.readDouble(); auto d1 = r.readDouble(); auto d2 = r.readDouble();
        line("OR " + hx(d0) + " " + hx(d1) + " " + hx(d2));
        auto z0 = r.readBoolean(); auto z1 = r.readBoolean();
        line(std::string("OR ") + B(z0) + " " + B(z1));
        for (int i = 0; i < 10; i++) line("OR uleb " + S(r.readULEB128()));
        auto t0 = r.readString(); auto t1 = r.readString(); auto t2 = r.readString(); auto t3 = r.readString();
        line("OR str [" + t0 + "] [" + t1 + "] [" + t2 + "] [" + t3 + "]");
        line("OR str " + S(java::utf16Length(r.readString())));   // ASCII only
        for (int i = 0; i < 4; i++) line("OR date " + S(r.readDate()));
    }

    const char* cases[] = {"", "01", "0102", "010203", "01020304", "0b", "0b05", "0b0568656c6c", "0b0568656c6c6f", "00", "0a", "0b00", "0b8001",
        "ffffffff0f", "ffffffffff0f", "80808080", "0b03e4b8ad", "0b06e4b8ade69687", "0bff", "0b0368ff69", "0b81808080", "0bffffffff0f", "0b80808080",
        "0b02c328", "0b03e082a9", "0b03eda080", "0b03f09f98", "0b04f4908080", "0b02c080", "0b01c3", "0b03e4b8", "0b0480ffc2c2", "0b06f09f9880f09f", "0b04f09f9880"};
    for (const char* hex : cases) {
        std::string h = hex, b;
        for (size_t i = 0; i < h.size(); i += 2) b += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
        std::string o = "OC " + h;
        for (int k = 0; k < 6; k++) o += " " + tryRead(b, k);
        line(o);
    }
}

static void utf8fuzz() {
    static const int pool[] = {0x41, 0x7f, 0x80, 0x8f, 0x9f, 0xa0, 0xbf, 0xc0, 0xc1, 0xc2, 0xdf, 0xe0, 0xe1, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf3,
                               0xf4, 0xf5, 0xff, 0x90, 0xa9};
    for (int i = 0; i < 6000; i++) {
        int n = 1 + rnd(9);
        std::string b;
        b += static_cast<char>(0x0b);
        b += static_cast<char>(n);
        for (int k = 0; k < n; k++) b += static_cast<char>(pool[rnd(25)]);
        line("OF " + u8(b) + " " + tryRead(b, 4));
    }
}

int main(int argc, char** argv) {
    std::string path = "tests/golden/expected2.txt";
    bool dump = false;
    for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--dump")) dump = true; else path = argv[i]; }
    ease(); animValue(); slide(); lazer(); kinetic(); user(); grades(); stats(); health(); utils(); io(); utf8fuzz();
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
    std::printf(bad ? "golden2: %d of %d lines differ\n" : "golden2: all %d lines match\n", bad ? bad : n, n);
    return bad ? 1 : 0;
}
