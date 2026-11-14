// Twelfth golden suite: regenerates the output of tests/golden/storyboard/StoryboardGolden.java with the C++ port (Storyboard) and diffs it
// against tests/golden/expected12.txt, which was produced by the REAL Java Storyboard + AnimationEquation (gen12.sh). Statement order mirrors the
// Java line for line (Java evaluates arguments left to right, C++ does not: every rnd() is sequenced explicitly).
// usage: opsu_golden12 [expected12.txt] [--dump]
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include "opsu/storyboard/Storyboard.h"

using namespace opsu;
using SB = Storyboard;

static std::string out;
static uint64_t seed_ = 20261113ULL * 7919ULL;
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
static std::string b(bool v) { return v ? "1" : "0"; }
static std::string jb(bool v) { return v ? "true" : "false"; }

static const float ALPHAS[] = {0.f, 0.25f, 0.5f, 1.f, 1.5f, 2.25f, 3.75f};

static int width(int p) { return p == SB::VECTOR_SCALE ? 2 : p == SB::COLOUR ? 3 : 1; }

static float val(int p) {
    int r;
    switch (p) {
        case SB::X: case SB::Y: r = rnd(1000); return (r - 200) / 2.f;
        case SB::SCALE: r = rnd(40); return (1 + r) / 10.f;
        case SB::VECTOR_SCALE: r = rnd(30); return (1 + r) / 10.f;
        case SB::ROTATION: r = rnd(720); return (r - 360) / 10.f;
        case SB::COLOUR: r = rnd(256); return r / 255.f;
        case SB::ALPHA: r = rnd(7); return ALPHAS[r];
        default: r = rnd(2); return static_cast<float>(r);
    }
}

static void addRandom(SB::CommandGroup& g) {
    int p = rnd(SB::PROPERTY_COUNT);
    int eq = rnd(ANIMATION_EQUATION_COUNT);
    int s0 = rnd(6000);
    int half = rnd(4);
    double start = s0 + (half == 0 ? 0.5 : 0.0);
    int kind = rnd(5);
    double dur;
    if (kind == 0) { int d = rnd(300); dur = -d; }
    else if (kind == 1) dur = 0;
    else { int d = rnd(2500); dur = d; }
    double end = start + dur;
    int n = width(p);
    std::vector<float> v0(static_cast<size_t>(n)), v1(static_cast<size_t>(n));
    for (int i = 0; i < n; i++) v0[static_cast<size_t>(i)] = val(p);
    for (int i = 0; i < n; i++) v1[static_cast<size_t>(i)] = val(p);
    g.add(p, static_cast<AnimationEquation>(eq), start, end, v0, v1);
}

static void sprites() {
    for (int c = 0; c < 150; c++) {
        int ox = rnd(3), oy = rnd(3);
        int ix = rnd(640), iy = rnd(480);
        int ncmd = (rnd(10) == 0) ? 0 : rnd(13);
        int nloops = rnd(3);
        int ntrig = rnd(2);
        SB::Sprite sp("sprite" + S(c) + ".png", ox, oy, static_cast<float>(ix), static_cast<float>(iy), false, 0, 0, false);
        for (int i = 0; i < ncmd; i++) addRandom(sp.commands);
        int totalLoopCmds = 0;
        for (int l = 0; l < nloops; l++) {
            int ls = rnd(4000);
            int rc = rnd(8) - 2;
            SB::LoopGroup& g = sp.addLoopGroup(ls, rc);
            int k = rnd(5);
            for (int i = 0; i < k; i++) addRandom(g);
            totalLoopCmds += k;
        }
        for (int t = 0; t < ntrig; t++) {
            SB::TriggerGroup& g = sp.addTriggerGroup("Trigger" + S(t), 100, 200, t);
            int k = rnd(3);
            for (int i = 0; i < k; i++) addRandom(g);
        }
        sp.finish();
        line("SP " + S(c) + " " + S(ncmd) + " " + S(nloops) + " " + S(totalLoopCmds) + " " + S(ntrig) + " " + b(sp.hasCommands()) + " " + b(sp.isDrawable()) + " " +
             hx(sp.getStartTime()) + " " + hx(sp.getEndTimeForDisplay()) + " " + hx(sp.getEarliestTransformTime()));
        double times[26];
        for (int i = 0; i < 20; i++) {
            int r = rnd(8000);
            int q = rnd(3);
            times[i] = (r - 800) + (q == 0 ? 0.25 : 0.0);
        }
        int ti = 20;
        for (int p = 0; p < SB::PROPERTY_COUNT && ti < 26; p++)
            for (const SB::Command& cm : sp.commands.lists[static_cast<size_t>(p)]) {
                if (ti >= 26) break;
                int idx = ti++;                                 // Java: times[ti++] = (ti % 2 == 0) ? ... : ... (the condition sees the incremented ti)
                times[idx] = (ti % 2 == 0) ? cm.start : cm.end;
            }
        while (ti < 26) times[ti++] = 2500.5;
        SB::State s;
        for (int i = 0; i < 26; i++) {
            double t = times[i];
            sp.evaluate(t, s);
            line("EV " + S(c) + " " + hx(t) + " " + b(sp.isAlive(t)) + " " + hx(s.x) + " " + hx(s.y) + " " + hx(s.scale) + " " + hx(s.vsx) + " " + hx(s.vsy) + " " +
                 hx(s.rotation) + " " + hx(s.r) + " " + hx(s.g) + " " + hx(s.b) + " " + hx(s.alpha) + " " + b(s.additive) + " " + b(s.flipH) + " " + b(s.flipV));
        }
    }
}

static void special() {
    SB::Sprite a("a.png", 1, 1, 10.f, 20.f, false, 0, 0, false);
    a.commands.add(SB::X, AnimationEquation::LINEAR, 1000, 500, {5.f}, {50.f});
    a.finish();
    SB::State s;
    for (double t : {0.0, 499.0, 500.0, 999.0, 1000.0, 1001.0, 5000.0}) { a.evaluate(t, s); line("SPEC1 " + hx(t) + " " + hx(s.x) + " " + b(a.isAlive(t))); }
    line("SPEC1 life " + hx(a.getStartTime()) + " " + hx(a.getEndTimeForDisplay()));
    SB::LoopGroup& big = a.addLoopGroup(0, std::numeric_limits<int>::max());
    SB::LoopGroup& neg = a.addLoopGroup(0, -5);
    line("SPEC2 " + S(big.totalIterations) + " " + S(neg.totalIterations));
    SB::Sprite l("l.png", 1, 1, 0.f, 0.f, false, 0, 0, false);
    SB::LoopGroup& g = l.addLoopGroup(1000, 2);
    g.add(SB::ALPHA, AnimationEquation::LINEAR, 0, 500, {0.f}, {1.f});
    g.add(SB::X, AnimationEquation::OUT_QUAD, 0, 250, {0.f}, {100.f});
    l.finish();
    line("SPEC3 life " + hx(l.getStartTime()) + " " + hx(l.getEndTimeForDisplay()) + " " + hx(l.getEarliestTransformTime()));
    for (double t = 900; t <= 2700; t += 100) { l.evaluate(t, s); line("SPEC3 " + hx(t) + " " + b(l.isAlive(t)) + " " + hx(s.alpha) + " " + hx(s.x)); }
    SB::Sprite e("e.png", 0, 0, 3.f, 4.f, false, 0, 0, false);
    e.finish();
    line("SPEC4 " + b(e.hasCommands()) + " " + hx(e.getStartTime()) + " " + hx(e.getEndTimeForDisplay()) + " " + b(e.isAlive(0)));
    e.evaluate(10, s);
    line("SPEC4 " + hx(s.x) + " " + hx(s.y) + " " + hx(s.scale) + " " + hx(s.alpha) + " " + b(s.additive));
    SB::Sprite f("f.png", 0, 0, 0.f, 0.f, false, 0, 0, false);
    f.commands.add(SB::FLIP_H, AnimationEquation::LINEAR, 100, 100, {1.f}, {1.f});
    f.commands.add(SB::BLEND, AnimationEquation::LINEAR, 200, 400, {1.f}, {0.f});
    f.finish();
    for (double t : {0.0, 99.0, 100.0, 150.0, 200.0, 399.0, 400.0, 500.0}) { f.evaluate(t, s); line("SPEC5 " + hx(t) + " " + b(s.flipH) + " " + b(s.additive)); }
    SB::Sprite al("al.png", 0, 0, 0.f, 0.f, false, 0, 0, false);
    al.commands.add(SB::ALPHA, AnimationEquation::LINEAR, 0, 1000, {0.f}, {3.5f});
    al.finish();
    for (double t : {0.0, 100.0, 250.0, 300.0, 500.0, 700.0, 999.0, 1000.0, 2000.0}) { al.evaluate(t, s); line("SPEC6 " + hx(t) + " " + hx(s.alpha)); }
    SB::Sprite tie("tie.png", 0, 0, 0.f, 0.f, false, 0, 0, false);
    SB::LoopGroup& tg = tie.addLoopGroup(500, 3);
    tg.add(SB::X, AnimationEquation::LINEAR, 0, 100, {1.f}, {2.f});
    tg.add(SB::X, AnimationEquation::LINEAR, 0, 100, {10.f}, {20.f});
    tg.add(SB::Y, AnimationEquation::LINEAR, 0, 200, {5.f}, {6.f});
    tie.finish();
    for (double t : {400.0, 500.0, 550.0, 600.0, 650.0, 700.0, 800.0, 1000.0, 1300.0}) { tie.evaluate(t, s); line("SPEC7 " + hx(t) + " " + hx(s.x) + " " + hx(s.y)); }
    SB::Sprite many("many.png", 0, 0, 0.f, 0.f, false, 0, 0, false);
    for (int i = 0; i < 60; i++) many.commands.add(SB::X, AnimationEquation::LINEAR, 100, 100, {static_cast<float>(i)}, {static_cast<float>(i)});
    many.finish();
    for (double t : {50.0, 100.0, 150.0}) { many.evaluate(t, s); line("SPEC8 " + hx(t) + " " + hx(s.x)); }
}

static std::string order(SB& sb) {
    std::string o;
    for (SB::Layer* l : sb.getDrawOrder()) o += l->name + ":" + S(l->depth) + " ";
    return o;
}

static void layers() {
    SB sb;
    line("LAY1 " + order(sb));
    SB::Layer& x = sb.getLayer("Custom1");
    SB::Layer& y = sb.getLayer("Custom2");
    SB::Layer& x2 = sb.getLayer("Custom1");
    line("LAY2 " + S(x.depth) + " " + S(y.depth) + " " + jb(&x == &x2) + " " + jb(x.masking) + " " + jb(x.visibleWhenPassing) + jb(x.visibleWhenFailing));
    line("LAY3 " + order(sb));
    sb.finish();
    line("LAY4 " + order(sb));
    SB::Layer& fail = sb.getLayer("Fail");
    SB::Layer& pass = sb.getLayer("Pass");
    line("LAY5 " + jb(fail.visibleWhenPassing) + jb(fail.visibleWhenFailing) + " " + jb(pass.visibleWhenPassing) + jb(pass.visibleWhenFailing));
    line("LAY6 " + jb(sb.hasDrawable()) + " " + jb(sb.hasSamples()) + " " + jb(sb.widescreen) + " " + jb(sb.useSkinSprites));
    auto sp = std::make_unique<SB::Sprite>("BG/Back.PNG", 1, 1, 320.f, 240.f, false, 0, 0, false);
    sp->commands.add(SB::ALPHA, AnimationEquation::LINEAR, 0, 10, {0.f}, {1.f});
    sb.getLayer("Background").sprites.push_back(std::move(sp));
    sb.getLayer("Foreground").samples.emplace_back("hit.wav", 1000, 80);
    sb.finish();
    line("LAY7 " + jb(sb.hasDrawable()) + " " + jb(sb.hasSamples()));
    line("LAY8 " + jb(sb.replacesBackground(std::string("bg/back.png"))) + " " + jb(sb.replacesBackground(std::string("BG/BACK.PNG"))) + " " +
         jb(sb.replacesBackground(std::string("bg/back.jpg"))) + " " + jb(sb.replacesBackground(std::nullopt)) + " " + jb(sb.replacesBackground(std::string(""))));
    SB sb2;
    sb2.getLayer("A").sprites.push_back(std::make_unique<SB::Sprite>("x", 0, 0, 0.f, 0.f, false, 0, 0, false));
    line("LAY9 " + jb(sb2.hasDrawable()));
}

int main(int argc, char** argv) {
    std::string path = "tests/golden/expected12.txt";
    bool dump = false;
    for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--dump")) dump = true; else path = argv[i]; }
    sprites(); special(); layers();
    if (dump) { std::cout << out; return 0; }

    std::ifstream f(path);
    if (!f) { std::printf("cannot open %s\n", path.c_str()); return 2; }
    std::stringstream expSs; expSs << f.rdbuf();
    std::istringstream a(expSs.str()), bb(out);
    std::string la, lb;
    int n = 0, bad = 0;
    while (true) {
        bool ga = static_cast<bool>(std::getline(a, la)), gb = static_cast<bool>(std::getline(bb, lb));
        if (!ga && !gb) break;
        n++;
        if (!ga || !gb || la != lb) {
            if (bad++ < 12) std::printf("MISMATCH line %d\n  java: %s\n  c++ : %s\n", n, (ga ? la : "<eof>").c_str(), (gb ? lb : "<eof>").c_str());
        }
    }
    std::printf(bad ? "golden12: %d of %d lines differ\n" : "golden12: all %d lines match\n", bad ? bad : n, n);
    return bad ? 1 : 0;
}
