// Golden test: regenerates the output of tests/golden/java/Golden.java with the C++ port and
// diffs it against tests/golden/expected.txt (produced by the REAL Java classes).
// usage: opsu_golden [expected.txt] [--dump]
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "opsu/GameMod.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/beatmap/TimingPoint.h"
#include "opsu/objects/curves/Curve.h"
#include "opsu/replay/ReplayFrame.h"
#include "opsu/util/JavaCompat.h"

using namespace opsu;
namespace J = opsu::java;

static std::string out;
static uint64_t seed_ = 12345;
static int rnd() { seed_ = seed_ * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<int>(seed_ >> 33); }
static int rnd(int n) { return rnd() % n; }
static std::string hx(float f) { uint32_t u; std::memcpy(&u, &f, 4); char b[16]; std::snprintf(b, sizeof b, "%08x", u); return b; }
static void line(const std::string& s) { out += s; out += '\n'; }
static std::string S(long long v) { return std::to_string(v); }
static const char* B(bool b) { return b ? "true" : "false"; }
static std::string hexList(const std::vector<float>& a, bool isNull = false) {
    if (isNull) return "-";
    std::string s;
    for (float f : a) s += hx(f) + ",";
    return s;
}

static void fmt() {
    std::vector<float> v;
    float cur[] = {0.f, -0.f, 0.125f, 0.375f, 0.625f, 0.875f, 1.005f, 2.5f, 0.5f, 1e7f, 9999999.f, 1e-3f, 0.00099999f,
                   100000.f, 999999.5f, 1e-5f, 1234567.f, 123456.f, 1000.f, 60000.f, 333.33334f, 428.57143f, 0.0625f,
                   1.0E10f, 3.4028235E38f, 1.4E-45f, 256.125f, 0.045f, 99.995f, 0.9999995f, 5e-6f, -7.5f, 12345.678f,
                   180.f, 123.456f, 0.1f, 1.f / 3.f};
    for (float f : cur) v.push_back(f);
    for (int i = -24; i <= 24; i++) v.push_back(static_cast<float>(i) / 8.f);
    for (int i = 0; i <= 200; i += 7) v.push_back(static_cast<float>(i) * 0.01f);
    for (int i = 0; i < 400; i++) {
        uint32_t sign = static_cast<uint32_t>(rnd(2));
        uint32_t ex = static_cast<uint32_t>(100 + rnd(60));
        uint32_t mant = static_cast<uint32_t>(rnd() & 0x7fffff);
        uint32_t bits = (sign << 31) | (ex << 23) | mant;
        float f; std::memcpy(&f, &bits, 4);
        v.push_back(f);
    }
    for (float f : v) {
        double d = f;
        line("FMT " + hx(f) + " " + J::formatFixed(d, 2) + "|" + J::formatFixed(d, 3) + "|" + J::formatGeneral(d) + "|" +
             J::floatToString(f) + "|" + J::formatTrimmed(d, 5));
    }
    for (float f : {std::nanf(""), INFINITY, -INFINITY})
        line("FMTSPECIAL " + J::formatFixed(f, 2) + "|" + J::formatGeneral(f) + "|" + J::floatToString(f));
}

static void parse() {
    const char* fl[] = {" 1.5 ", "1e3", "1.5f", ".5", "5.", "+3", "-0", "NaN", "-Infinity", "abc", "", "1e", "1.5.2", "  ",
                        "1e40", "1e-50", "3.4028236e38", "0.1", "16777217", "1,5", "1_0", "-.5e2", "7d", "+", "e5", "1 2"};
    for (const char* s : fl) {
        std::string r;
        try { r = hx(J::parseFloat(s)); } catch (...) { r = "ERR"; }
        line(std::string("PF [") + s + "] " + r);
    }
    const char* in[] = {"12", "-0", "+5", "", "1 ", " 1", "2147483648", "-2147483648", "2147483647", "-", "+", "0x10", "1.0", "007", "99999999999"};
    for (const char* s : in) {
        std::string r1, r2, r3;
        try { r1 = S(J::parseInt(s)); } catch (...) { r1 = "ERR"; }
        try { r2 = S(J::parseByte(s)); } catch (...) { r2 = "ERR"; }
        try { r3 = S(J::parseShort(s)); } catch (...) { r3 = "ERR"; }
        line(std::string("PI [") + s + "] " + r1 + " " + r2 + " " + r3);
    }
    const char* sp[] = {"", ",", ",,", "a", "a,b", "a,,b", ",a", "a,", "a,,", ",,a,,", "a,b,c,,,"};
    for (const char* s : sp) {
        auto p = J::split(s, ',');
        std::string b;
        for (auto& x : p) b += "<" + x + ">";
        line(std::string("SPLIT [") + s + "] " + S(static_cast<long long>(p.size())) + " " + b);
    }
    const char* sp2[] = {"B|1:2|3:4", "B", "|", "||a", "0:0:0:0:", ":"};
    for (const char* s : sp2)
        line(std::string("SPLIT2 [") + s + "] " + S(static_cast<long long>(J::split(s, '|').size())) + " " + S(static_cast<long long>(J::split(s, ':').size())));
}

static void tp() {
    const char* lines[] = {"0,1000.0,4,1,0,100,0,0", "70382,\t-66.6666666666667,4,2,1,70,0,1", "1000,500,4,2,1,70", "1000,500,4,2,1,70,1", "1000,500", "1000",
                           "1000.7,333.33334,3,3,2,50,1,1", "-5,-100,4,1,0,100,1,0", "0,-5000,4,1,0,100,1,0", "0,-1,4,1,0,100,1,0", "0,1e-3,4,1,0,100,1,0",
                           "0,1000000,4,1,0,100,1,0", "0,123456.789,4,1,0,100,1,1", "0,60000,4,1,0,100", "0,0,4,1,0,100", "abc,1,2,3,4,5", "0,1,x,3,4,5", "0,1,2,300,4,5",
                           "0,428.571428,4,1,0,100,1,2", "0,-100.5,4,1,0,100,0,0", "0,0.1,4,1,0,100,0,0", "0,99999.9,4,1,0,100,0,0"};
    for (const char* l : lines) {
        std::string r;
        try {
            TimingPoint t(l);
            r = t.toString() + " | " + S(t.getTime()) + " " + hx(t.getBeatLength()) + " " + hx(t.getSliderMultiplier()) + " " + S(t.getMeter()) + " " +
                S(t.getSampleType()) + " " + S(t.getSampleTypeCustom()) + " " + hx(t.getSampleVolume()) + " " + B(t.isInherited()) + " " + B(t.isKiaiTimeActive());
        } catch (...) { r = "ERR"; }
        line(std::string("TP [") + l + "] " + r);
    }
    line("TPDEFAULT " + TimingPoint::DEFAULT.toString());
}

static const std::vector<std::string> HO = {
    "256,148,9466,1,2,0:0:0:0:", "256,148,9466,5,2", "256,148,9466,1,0", "100.5,50.25,1000,1,0,3:2:1:80:hit.wav", "100,100,1,1,0,1:2",
    "100,100,1,1,0,1:2:3:4:a:b", "100,100,1,1,0,1", "100,100,1,1,0,1:2:3:4:", "100,100,1,1,0,1:2:3:4::x",
    "256,192,654,12,0,4029,0:0:0:0:", "256,192,654,12,0,4029:1:2", "256,192,654,12,0,4029", "256,192,654,8,0,4029,1:2", "256,192,654,8,0,4029,",
    "300,68,4591,2,0,B|372:100|332:172|420:192,2,180,2|2|2,0:0|0:0|0:0,0:0:0:0:", "300,68,4591,6,0,B|372:100|332:172|420:192,2,180.5",
    "300,68,4591,2,4,L|372:100|332:172,1,100,2|8,1:2|3:4,1:2:3:50:", "100,100,500,2,0,P|150:150|200:100,1,150.75",
    "100,100,500,2,0,C|150:150|200:100|250:150,1,200", "100,100,500,2,0,B|150:150|150:150|200:200|300:100,1,300,0|0,0:0|0:0",
    "100,100,500,2,0,P|150:150|200:200,1,150", "100,100,500,2,0,P|200:100|300:100,1,150", "100,100,500,2,0,P|100:100|300:100,1,150",
    "100,100,500,2,0,B|200:100,1,2", "100,100,500,2,0,B,1,100", "100,100,500,2,0,C|150:150,1,100", "100,100,500,2,0,L|1:1,1,1e3",
    "100,100,500,2,0,B|200:100|200:100|300:50|300:50|400:100,1,250", "100,100,500,2,0,B|100:100|200:100|200:100,1,90",
    "100,100,500,2,0,B|100:100|100:100,1,90", "100,100,500,2,0,L|100:100|100:100|100:100,1,90",
    "0,0,0,1,2", "bad", "", "1,2", "1,2,3,4", "1,2,3,4,300", "1,2,3,4,-3", "1,2,3,16,0", "1,2,3,2,0,B,1,100",
    "1.5f,2,3,1,0", "x,2,3,1,0", " 1 ,2,3,1,0", "0.00001,0.5,3,1,0", "-0.125,-3.5,3,1,0", "1234567.123456,0.000004,3,1,0",
    "100,100,500,2,0,B|1:1,1,1e10", "100,100,500,2,0,B|1:1,1,12345.678"};

static const int CFG[6][3] = {{1920, 1080, 10}, {1280, 720, 16}, {800, 600, 10}, {1080, 2400, 10}, {640, 480, 20}, {512, 384, 13}};

static void setHR(bool on) { if (GameMod::HARD_ROCK.isActive() != on) GameMod::HARD_ROCK.toggle(false); }
static void init(const int* c) { HitObject::init(c[0], c[1], static_cast<float>(c[2]) / 10.f); }

static void ho() {
    GameMod::loadModState(0);
    for (const std::string& l : HO) {
        std::unique_ptr<HitObject> hp;
        try { hp = std::make_unique<HitObject>(l); } catch (...) { line("HO [" + l + "] ERR"); continue; }
        HitObject& h = *hp;
        bool sl = h.isSlider();  // Java arrays are null (printed "-") only for non-sliders
        line("HO [" + l + "] " + h.toString() + " | " + h.getTypeName() + " t=" + S(h.getTime()) + " ty=" + S(h.getType()) + " hs=" + S(h.getHitSoundType()) +
             " e=" + S(h.getEdgeHitSoundType(-1)) + "," + S(h.getEdgeHitSoundType(0)) + "," + S(h.getEdgeHitSoundType(7)) +
             " ss=" + S(h.getSampleSet(0)) + "," + S(h.getSampleSet(3)) + " as=" + S(h.getAdditionSampleSet(0)) + "," + S(h.getAdditionSampleSet(9)) +
             " os=" + S(h.getObjectSampleSet()) + "," + S(h.getObjectAdditionSampleSet()) + " ci=" + S(h.getCustomSampleIndex()) + " vol=" + S(h.getHitSoundVolume()) +
             " file=" + h.getHitSoundFile() + " rep=" + S(h.getRepeatCount()) + " pl=" + hx(h.getPixelLength()) +
             " et=" + S(h.getEndTime()) + " skip=" + S(h.getComboSkip()) + " nc=" + B(h.isNewCombo()) + " st=" + S(static_cast<int>(h.getSliderType())) +
             " sx=" + hexList(h.getSliderX(), !sl) + " sy=" + hexList(h.getSliderY(), !sl) + " x=" + hx(h.getX()) + " y=" + hx(h.getY()) +
             " time=" + hx(h.getSliderTime(1.4f, 333.33f)));
        for (const auto& c : CFG) {
            init(c);
            line("HOINIT " + S(c[0]) + "x" + S(c[1]) + "@" + S(c[2]) + " " + hx(HitObject::getXMultiplier()) + " " + hx(HitObject::getYMultiplier()) + " " +
                 S(HitObject::getXOffset()) + " " + S(HitObject::getYOffset()));
            for (int mode = 0; mode < 3; mode++) {
                setHR(mode == 1 || mode == 2);
                HitObject::setStackOffset(mode == 2 ? 3.2f : 0.f);
                h.setStack(mode == 2 ? 3 : 0);
                line("HOSCALE " + S(mode) + " " + hx(h.getScaledX()) + " " + hx(h.getScaledY()) + " " + hexList(h.getScaledSliderX(), !sl) + " " + hexList(h.getScaledSliderY(), !sl));
            }
            setHR(false); HitObject::setStackOffset(0.f); h.setStack(0);
        }
    }
}

static size_t curated;
static std::string fnv(const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : s) { h ^= c; h *= 0x100000001b3ULL; }
    char b[24]; std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h)); return b;
}

static std::vector<std::string> sliders() {
    std::vector<std::string> l;
    for (const std::string& s : HO)
        if (J::split(s, ',').size() > 7 && s.find('|') != std::string::npos && s.find('|') > 0 && s.find("1e10") == std::string::npos) l.push_back(s);
    curated = l.size();
    const char* types = "BLPC";
    for (int i = 0; i < 300; i++) {
        char t = types[rnd(4)];
        int n = 1 + rnd(6);
        if (t == 'P' && rnd(4) != 0) n = 2;
        std::string b;
        b += S(rnd(513)); b += ',';
        b += S(rnd(385)); b += ",1000,2,0,"; b += t;
        for (int k = 0; k < n; k++) {
            if (rnd(6) == 0 && k > 0) { std::string last = b.substr(b.rfind('|') + 1); b += '|'; b += last; }
            else { b += '|'; b += S(rnd(513)); b += ':'; b += S(rnd(385)); }
        }
        int len = 500 + rnd(80000);
        b += ','; b += S(1 + rnd(3)); b += ','; b += S(len / 100); b += '.';
        char two[8]; std::snprintf(two, sizeof two, "%02d", len % 100); b += two;
        l.push_back(b);
    }
    return l;
}

static void curves() {
    GameMod::loadModState(0);
    std::vector<std::string> sl = sliders();
    int idx = 0;
    for (const std::string& l : sl) {
        idx++;
        std::unique_ptr<HitObject> hp;
        try { hp = std::make_unique<HitObject>(l); } catch (...) { continue; }
        for (int cfg = 0; cfg < 3; cfg++) {
            init(CFG[cfg == 2 ? 1 : 0]);
            bool scaled = cfg != 1;
            std::string r;
            try {
                auto c = hp->getSliderCurve(scaled);
                const auto& p = c->getCurvePoints();
                std::string b = "n=" + S(static_cast<long long>(p.size())) + " sa=" + hx(c->getStartAngle()) + " ea=" + hx(c->getEndAngle()) + " pts=";
                for (const Vec2f& v : p) b += hx(v.x) + ":" + hx(v.y) + " ";
                b += "at=";
                for (int k = 0; k <= 10; k++) { Vec2f v = c->pointAt(static_cast<float>(k) / 10.f); b += hx(v.x) + ":" + hx(v.y) + " "; }
                b += "ix=";
                for (float t : {-0.5f, 0.f, 0.33f, 1.f, 1.5f}) b += hx(c->indexAt(t)) + " ";
                b += "x0=" + hx(c->getX(0)) + " y1=" + hx(c->getY(1));
                r = b;
                if (static_cast<size_t>(idx) > curated) r = "n=" + S(static_cast<long long>(p.size())) + " hash=" + fnv(r);
            } catch (...) { r = "ERR"; }
            line("CURVE " + S(idx) + " cfg=" + S(cfg) + " " + r);
        }
    }
}

static void replay() {
    init(CFG[0]);
    for (int i = 0; i < 300; i++) {
        float x = (i % 3 == 0) ? static_cast<float>(rnd(8000) - 200) / 8.f : static_cast<float>(rnd(51200)) / 100.f;
        float y = (i % 3 == 1) ? static_cast<float>(rnd(8000) - 200) / 8.f : static_cast<float>(rnd(38400)) / 100.f;
        int td = rnd(100);
        int tm = rnd(100000);
        int keys = rnd(4096);
        ReplayFrame f(td, tm, x, y, keys);
        line("RF " + f.toString() + " " + S(f.getScaledX()) + " " + S(f.getScaledY()) + " " + B(f.isKeyPressed()) + " " + S(f.getKeys()));
    }
    ReplayFrame s = ReplayFrame::getStartFrame(77);
    line("RFSTART " + s.toString() + " " + S(s.getTimeDiff()));
}

static std::string esc(std::string s) {
    for (size_t p; (p = s.find('\n')) != std::string::npos;) s.replace(p, 1, "\\n");
    return s;
}

static void mods() {
    const auto& vals = GameMod::values();
    for (GameMod* m : vals)
        line("MODINFO " + m->getName() + " [" + m->getAbbreviation() + "] " + S(m->getBit()) + " " + S(m->getKey()) + " " + hx(m->getMultiplier()) + " " + esc(m->getDescription()));
    for (int a = 0; a < GameMod::SIZE; a++)
        for (int b = 0; b < GameMod::SIZE; b++)
            for (int inv = 0; inv < 2; inv++) {
                GameMod::loadModState(0);
                vals[a]->toggle(inv == 1); vals[b]->toggle(inv == 1);
                line("MODPAIR " + S(a) + " " + S(b) + " " + S(inv) + " " + S(GameMod::getModState()));
            }
    GameMod::loadModState(0);
    for (int s = 0; s < 3000; s++) {
        if (s % 5 == 0) GameMod::loadModState(0);
        GameMod* m = vals[rnd(GameMod::SIZE)];
        m->toggle(rnd(5) != 0);
        line("MODSEQ " + S(s) + " " + S(GameMod::getModState()) + " " + hx(GameMod::getScoreMultiplier()) + " " + hx(GameMod::getSpeedMultiplier()) + " " + hx(GameMod::getDifficultyMultiplier()));
    }
    for (int st = 0; st < 16384; st += 31) {
        GameMod::loadModState(st);
        line("MODLOAD " + S(st) + " " + S(GameMod::getModState()) + " " + hx(GameMod::getScoreMultiplier()) + " " + hx(GameMod::getSpeedMultiplier()) + " " +
             hx(GameMod::getDifficultyMultiplier()) + " " + GameMod::getModString(st));
    }
    line("MODSTRING0 " + GameMod::getModString(0));
}

int main(int argc, char** argv) {
    std::string path = "tests/golden/expected.txt";
    bool dump = false;
    for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--dump")) dump = true; else path = argv[i]; }
    fmt(); parse(); tp(); ho(); curves(); replay(); mods();
    if (dump) { std::cout << out; return 0; }

    std::ifstream f(path);
    if (!f) { std::printf("cannot open %s\n", path.c_str()); return 2; }
    std::stringstream expSs; expSs << f.rdbuf();
    std::string exp = expSs.str();
    std::istringstream a(exp), b(out);
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
    std::printf(bad ? "golden: %d of %d lines differ\n" : "golden: all %d lines match\n", bad ? bad : n, n);
    return bad ? 1 : 0;
}
