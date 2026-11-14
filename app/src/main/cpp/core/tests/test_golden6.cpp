// Sixth golden suite: regenerates the output of tests/golden/java/Golden6.java with the C++ port (BeatmapGroup) and diffs it
// against tests/golden/expected6.txt, which was produced by the REAL BeatmapGroup.java (gen6.sh). Statement order mirrors the
// Java line for line: every rnd() call is sequenced explicitly (C++ does not evaluate operands left to right).
// usage: opsu_golden6 [expected6.txt] [--dump]
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapGroup.h"

using namespace opsu;

static std::string out;
static uint64_t seed_ = 20261113ULL;
static int rnd() { seed_ = seed_ * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<int>(seed_ >> 33); }
static int rnd(int n) { return rnd() % n; }
static std::string hx(float f) {
    uint32_t u;
    if (f != f) u = 0x7fc00000u; else std::memcpy(&u, &f, 4);
    char b[16]; std::snprintf(b, sizeof b, "%08x", u); return b;
}
static void line(const std::string& s) { out += s; out += '\n'; }

static void basics() {
    for (BeatmapGroup g : beatmapGroupValues()) {
        std::string r;
        try { r = hx(getTabCenterX(g)); } catch (const std::logic_error&) { r = "NPE"; }
        line(std::string("PREINIT ") + name(g) + " " + r);
    }
    std::string sb = "VALUES";
    for (BeatmapGroup g : beatmapGroupValues()) sb += std::string(" ") + name(g);
    sb += " REV";
    for (BeatmapGroup g : beatmapGroupValuesReversed()) sb += std::string(" ") + name(g);
    line(sb);
    line(std::string("CURRENT0 ") + name(currentGroup()));
    for (BeatmapGroup g : beatmapGroupValues()) {
        setGroup(g);
        const char* m = getEmptyMessage(g);
        line(std::string("SET ") + name(g) + " " + name(currentGroup()) + " " + (m ? m : "null"));
    }
    setGroup(BeatmapGroup::ALL);
}

static void filters() {
    for (int c = 0; c < 400; c++) {
        int style = c % 8;
        int n = (c % 10 == 0) ? rnd(3) : rnd(90);
        std::vector<BeatmapSetBeatmaps> sets(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) {
            int nb;
            if (style == 7 && rnd(5) == 0) nb = 0; else nb = 1 + rnd(4);
            for (int k = 0; k < nb; k++) {
                Beatmap b("x.osu");
                int r = rnd(10);
                int64_t t;
                if (style == 0) { if (r < 3) t = 0; else t = rnd(6); }
                else if (style == 1) { if (r < 2) t = 0; else t = 1000000000LL + rnd(1000000); }
                else if (style == 2) { if (r < 2) t = 0; else t = 1700000000000LL + static_cast<int64_t>(rnd(2000000000)); }
                else if (style == 3) {
                    if (r < 2) t = 0;
                    else { int64_t a = rnd(2000000000); int64_t m = rnd(500); t = 1600000000000LL + a * m; }
                }
                else if (style == 4) { if (r < 5) t = 0; else t = static_cast<int64_t>(rnd(2000000000)) * 7LL; }
                else if (style == 5) t = 1500000000000LL + static_cast<int64_t>(rnd(100)) * 4294967296LL;
                else if (style == 6) { if (r < 1) t = 0; else t = 9000000000000000000LL + rnd(1000); }
                else { if (r < 4) t = 0; else t = 1600000000000LL + static_cast<int64_t>(rnd(90)) * 86400000LL; }
                b.lastPlayed = t;
                b.favorite = rnd(12) == 0;
                sets[static_cast<size_t>(i)].push_back(b);
            }
        }
        std::vector<const BeatmapSetBeatmaps*> ptrs;
        for (const auto& s : sets) ptrs.push_back(&s);
        std::string sb = "FILTER " + std::to_string(c) + " " + std::to_string(n);
        for (BeatmapGroup g : beatmapGroupValues()) {
            sb += std::string(" |") + name(g);
            for (size_t idx : filter(g, ptrs)) sb += " " + std::to_string(idx);
        }
        line(sb);
    }
}

static void tabs() {
    for (int c = 0; c < 300; c++) {
        int cw = 100 + rnd(3900);
        float bottomY = rnd(2000) / 4.f - 50.f;
        int tw, th;
        if (c % 3 == 0) { tw = 1 + rnd(60); th = 1 + rnd(60); }
        else if (c % 3 == 1) { tw = 1 + rnd(cw / 2 + 1); th = 1 + rnd(200); }
        else { tw = cw / 3 + rnd(300); th = 1 + rnd(500); }
        std::string sb = "TAB " + std::to_string(c) + " " + std::to_string(cw) + " " + hx(bottomY) + " " + std::to_string(tw) + " " + std::to_string(th);
        for (BeatmapGroup g : beatmapGroupValues()) {
            init(g, cw, bottomY, tw, th);
            sb += " " + hx(getTabCenterX(g)) + " " + hx(getTabCenterY(g)) + " " + hx(getTabLeft(g));
        }
        line(sb);
    }
    // containerWidth = 6 * tabWidth makes tabOffset == tabWidth exactly (the `>` boundary), and +-1 either side
    for (int k = 10; k < 210; k += 10) {
        for (int d = -1; d <= 1; d++) {
            int tw = k + d, cw = 6 * k, th = 20 + k % 7;
            float bottomY = 1000.f;
            std::string sb = "TABEDGE " + std::to_string(cw) + " " + std::to_string(tw) + " " + std::to_string(th);
            for (BeatmapGroup g : beatmapGroupValues()) {
                init(g, cw, bottomY, tw, th);
                sb += " " + hx(getTabCenterX(g)) + " " + hx(getTabCenterY(g)) + " " + hx(getTabLeft(g));
            }
            line(sb);
        }
    }
}

int main(int argc, char** argv) {
    std::string path = "tests/golden/expected6.txt";
    bool dump = false;
    for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--dump")) dump = true; else path = argv[i]; }
    basics(); filters(); tabs();
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
    std::printf(bad ? "golden6: %d of %d lines differ\n" : "golden6: all %d lines match\n", bad ? bad : n, n);
    return bad ? 1 : 0;
}
