// Golden suite 11: replays tests/golden/cases11.txt (written by tests/golden/java/Golden11.java next to the expected output)
// through the C++ OsuToPjsk / java::trim / java::splitAll and diffs the produced text against
// tests/golden/expected11.txt, which came from the REAL Java OsuToPjsk + PjskChart (gen11.sh). The corpus holds the
// inputs, so nothing has to be mirrored: no random generator exists on the C++ side.
// usage: opsu_golden11 [expected11.txt] [cases11.txt] [--dump]
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "opsu/fourresonance/OsuToPjsk.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;
using namespace opsu::fourresonance;

static std::string out;
static int no = 0;

static std::string hx(double d) {
    uint64_t u;
    if (d != d) u = 0x7ff8000000000000ull; else std::memcpy(&u, &d, 8);
    char b[24]; std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u)); return b;
}
static std::string hx(float f) {
    uint32_t u;
    if (f != f) u = 0x7fc00000u; else std::memcpy(&u, &f, 4);
    char b[16]; std::snprintf(b, sizeof b, "%08x", u); return b;
}
static void line(const std::string& s) { out += s; out += '\n'; }
static std::string S(long long v) { return std::to_string(v); }
static const char* B(bool b) { return b ? "true" : "false"; }

static std::string hexOf(const std::string& s) {
    if (s.empty()) return "-";
    std::string r; char t[4];
    for (unsigned char c : s) { std::snprintf(t, sizeof t, "%02x", c); r += t; }
    return r;
}
static std::string unhex(const std::string& h) {
    std::string r;
    if (h == "-") return r;
    for (size_t i = 0; i + 1 < h.size(); i += 2) r.push_back(static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16)));
    return r;
}
static double dbl(const std::string& h) {
    uint64_t u = std::stoull(h, nullptr, 16);
    double d; std::memcpy(&d, &u, 8); return d;
}
static std::string ex(const std::exception& e) {
    if (dynamic_cast<const java::NumberFormatException*>(&e)) return "NFE";
    if (dynamic_cast<const java::NegativeArraySizeException*>(&e)) return "NAS";
    if (dynamic_cast<const std::out_of_range*>(&e)) return "OOB";
    return "EX";
}
static std::string ls(const std::array<float, 2>& f) { return hx(f[0]) + "/" + hx(f[1]); }

static void printChart(int i, const std::optional<PjskChart>& oc) {
    if (!oc) { line("CV " + S(i) + " null"); return; }
    const PjskChart& c = *oc;
    line("CV " + S(i) + " ok keys=" + S(c.keyCount) + " od=" + hx(c.overallDifficulty) + " offset=" + hx(c.offsetMs) + " nseg=" + S(c.bpmSegments.size()) +
         " nts=" + S(c.timeScale.size()) + " nn=" + S(c.notes.size()));
    for (size_t j = 0; j < c.bpmSegments.size(); j++) {
        const auto& s = c.bpmSegments[j];
        line("SEG " + S(i) + " " + S(j) + " " + hx(s.startMs) + " " + hx(s.beatAtStart) + " " + hx(s.beatLengthMs) + " " + hx(s.bpm));
    }
    for (size_t j = 0; j < c.timeScale.size(); j++) {
        const auto& s = c.timeScale[j];
        line("TS " + S(i) + " " + S(j) + " " + hx(s.beat) + " " + hx(s.timeMs) + " " + hx(s.timeScale));
    }
    for (size_t j = 0; j < c.notes.size(); j++) {
        const auto& n = c.notes[j];
        line("NOTE " + S(i) + " " + S(j) + " " + S(n.column) + " " + hx(n.lane) + " " + hx(n.size) + " " + hx(n.beat) + " " + hx(n.endBeat) + " " + S(n.timeMs) +
             " " + S(n.endTimeMs) + " " + B(n.hold) + " " + B(n.critical) + " " + S(static_cast<int>(n.tail)));
    }
}

static void runCase(const std::string& ln) {
    std::istringstream is(ln);
    std::string op;
    is >> op;
    if (op == "TS") {
        std::string h; is >> h;
        std::string s = unhex(h);
        std::string sp;
        std::vector<std::string> parts = java::splitAll(s, ',');
        sp = S(parts.size()) + ":";
        for (size_t k = 0; k < parts.size(); k++) { if (k > 0) sp += '|'; sp += hexOf(parts[k]); }
        line("TS " + S(no++) + " " + hexOf(java::trim(s)) + " " + sp);
    } else if (op == "BT") {
        int n; is >> n;
        std::vector<PjskChart::BpmSegment> segs;
        for (int k = 0; k < n; k++) {
            std::string a, b, c, d; is >> a >> b >> c >> d;
            segs.emplace_back(dbl(a), dbl(b), dbl(c), dbl(d));
        }
        int m; is >> m;
        int i0 = no++;
        for (int k = 0; k < m; k++) {
            std::string t; is >> t;
            double tv = dbl(t);
            line("BT " + S(i0) + " " + S(k) + " " + hx(tv) + " " + hx(OsuToPjsk::timeMsToBeat(tv, segs)) + " " + hx(OsuToPjsk::beatToTimeMs(tv, segs)) + " " +
                 hx(OsuToPjsk::detail::noteTimeMsToBeat(tv, segs)));
        }
    } else if (op == "LL") {
        int keys, steps; is >> keys >> steps;
        std::string res, ctor = "ok";
        std::unique_ptr<OsuToPjsk::LaneLayout> layout;
        try { layout = std::make_unique<OsuToPjsk::LaneLayout>(keys); } catch (const std::exception& e) { ctor = ex(e); }
        for (int s = 0; s < steps; s++) {
            int col, time, end; is >> col >> time >> end;
            std::string r;
            if (!layout) r = ctor;
            else { try { r = ls(layout->place(col, time, end)); } catch (const std::exception& e) { r = ex(e); } }
            res += " " + r;
        }
        line("LL " + S(no++) + " keys=" + S(keys) + " ctor=" + ctor + res);
    } else if (op == "CL") {
        int keys, col; is >> keys >> col;
        std::string r;
        try { r = ls(OsuToPjsk::columnToLaneSize(keys, col)); } catch (const std::exception& e) { r = ex(e); }
        line("CL " + S(no++) + " " + r);
    } else if (op == "TC") {
        int keys, col; is >> keys >> col;
        line("TC " + S(no++) + " " + B(OsuToPjsk::isTingedColumn(keys, col)));
    } else if (op == "CV") {
        int tail, tinged, minHold, maxSim; std::string h;
        is >> tail >> tinged >> minHold >> maxSim >> h;
        int i = no++;
        std::optional<PjskChart> c;
        try {
            c = OsuToPjsk::convertString(unhex(h), static_cast<PjskChart::TailMode>(tail), tinged != 0, minHold, maxSim);
        } catch (const std::exception& e) {
            line("CV " + S(i) + " " + ex(e));
            return;
        }
        printChart(i, c);
    } else {
        std::printf("unknown corpus op: %s\n", op.c_str());
        std::exit(2);
    }
}

int main(int argc, char** argv) {
    std::string expPath = "tests/golden/expected11.txt", casesPath = "tests/golden/cases11.txt";
    bool dump = false;
    int positional = 0;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--dump")) dump = true;
        else if (positional++ == 0) expPath = argv[i];
        else casesPath = argv[i];
    }
    std::ifstream cf(casesPath);
    if (!cf) { std::printf("cannot open %s\n", casesPath.c_str()); return 2; }
    std::string ln;
    int cases = 0;
    while (std::getline(cf, ln)) { if (ln.empty()) continue; runCase(ln); cases++; }
    if (dump) { std::cout << out; return 0; }

    std::ifstream f(expPath);
    if (!f) { std::printf("cannot open %s\n", expPath.c_str()); return 2; }
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
    std::printf(bad ? "golden11: %d of %d lines differ (%d cases)\n" : "golden11: all %d lines match (%d cases)\n", bad ? bad : n, bad ? n : cases, cases);
    return bad ? 1 : 0;
}
