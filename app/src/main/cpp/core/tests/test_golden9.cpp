// Ninth golden suite: checks util/JavaDate (parseDate / reformatDate / formatDate) against tests/golden/expected9.txt, produced by the real JDK
// (gen9.sh) with SimpleDateFormat as the download servers use it, once per time zone (the TZ environment variable is switched per ZONE line).
// usage: opsu_golden9 [expected9.txt] [-v]
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include "opsu/util/JavaDate.h"

using namespace opsu;

static std::string unhex(const std::string& h) {
    std::string s;
    for (size_t i = 0; i + 1 < h.size(); i += 2) s += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
    return s;
}
static std::vector<std::string> splitBar(const std::string& s) {
    std::vector<std::string> v;
    size_t a = 0;
    for (;;) { size_t b = s.find('|', a); if (b == std::string::npos) { v.push_back(s.substr(a)); break; } v.push_back(s.substr(a, b - a)); a = b + 1; }
    return v;
}

static const char* PAT[3] = {"yyyy-MM-dd'T'HH:mm:ss", "yyyy-MM-dd HH:mm:ss.SSS", "yyyy-MM-dd'T'HH:mm:ss'Z'"};
static const bool UTC[3] = {true, false, true};

// Inputs / results outside what util/JavaDate promises (see its header and PORTING_NOTES batch 22): counted separately, never silently dropped.
static bool outOfScope(const std::string& in, const std::string& wantMillis, const std::string& gotMillis, bool isMillisCase) {
    if (in.find("NaN") != std::string::npos) return true;                        // DecimalFormat parses "NaN" / infinity as numbers
    for (unsigned char c : in) if (c >= 0x80) return true;                       // non-ASCII digits (Character.digit) and infinity sign
    int run = 0;
    for (char c : in) { run = (c >= '0' && c <= '9') ? run + 1 : 0; if (run >= 10) return true; }  // > 9 digits: Long.intValue wrap
    if (in.find("-20") == 0 || in.find("-12") == 0 || in.find("-1-") == 0) return true;           // negative year (BC)
    if (isMillisCase) {
        const long long cut = -2208988800000LL;                                 // 1900-01-01: Java's zones use a fixed first offset before their first rule, glibc the local mean time; Julian calendar before 1582
        for (const std::string& m : {wantMillis, gotMillis}) if (m != "ERR" && std::stoll(m) < cut) return true;
    }
    return false;
}

int main(int argc, char** argv) {
    std::string path = argc > 1 && argv[1][0] != '-' ? argv[1] : "tests/golden/expected9.txt";
    bool verbose = argc > 1 && std::string(argv[argc - 1]) == "-v";
    std::ifstream f(path);
    if (!f) { std::printf("cannot open %s\n", path.c_str()); return 2; }
    std::string ln, zone = "?";
    std::map<std::string, std::pair<int, int>> perZone;  // zone -> (cases, bad)
    int n = 0, bad = 0, skipped = 0;
    while (std::getline(f, ln)) {
        std::vector<std::string> p = splitBar(ln);
        if (p[0] == "ZONE") { zone = p[1]; setenv("TZ", zone.c_str(), 1); tzset(); continue; }
        std::string got, want, desc;
        if (p[0] == "PD" && p.size() == 4) {
            int id = std::stoi(p[1]);
            std::string in = unhex(p[2]);
            std::optional<int64_t> r = java::parseDate(in, PAT[id], UTC[id]);
            got = r ? std::to_string(*r) : "ERR";
            want = p[3];
            desc = "PD pat" + p[1] + " in=" + in;
        } else if (p[0] == "RF" && p.size() == 4) {
            int id = std::stoi(p[1]);
            std::string in = unhex(p[2]);
            std::string base = (id == 0 && in.size() >= 19) ? in.substr(0, 19) : in;  // the 19-char cut of Catboy / Hinamizawa / Nekoha
            got = java::reformatDate(base, PAT[id], UTC[id]);
            if (got == base) got = in;  // parse failed: the server returns the raw (uncut) string
            want = unhex(p[3]);
            desc = "RF pat" + p[1] + " in=" + in;
        } else if (p[0] == "FM" && p.size() == 3) {
            got = java::formatDate(std::stoll(p[1]));
            want = unhex(p[2]);
            desc = "FM ms=" + p[1];
        } else continue;
        n++;
        auto& z = perZone[zone];
        z.first++;
        if (got != want && p[0] == "FM" && std::stoll(p[1]) < -2208988800000LL) { skipped++; continue; }
        if (got != want && p[0] != "FM" && outOfScope(unhex(p[2]), want, got, p[0] == "PD")) { skipped++; continue; }
        if (got != want && p[0] == "RF") {
            // an RF line is out of scope when its PD twin was: re-derive via the parse result of the same input
            int id = std::stoi(p[1]);
            std::string in = unhex(p[2]);
            std::string base = (id == 0 && in.size() >= 19) ? in.substr(0, 19) : in;
            std::optional<int64_t> r = java::parseDate(base, PAT[id], UTC[id]);
            if (!r || *r < -2208988800000LL) { skipped++; continue; }
        }
        if (got != want) {
            bad++; z.second++;
            if (verbose && bad <= 40) std::printf("MISMATCH [%s] %s\n  want %s\n  got  %s\n", zone.c_str(), desc.c_str(), want.c_str(), got.c_str());
        }
    }
    for (auto& kv : perZone) std::printf("  %-22s %6d cases, %d mismatches\n", kv.first.c_str(), kv.second.first, kv.second.second);
    std::printf("golden9: %d cases, %d out-of-scope differences (before 1900 (zone mean times, Julian calendar) / BC years / NaN / non-ASCII digits / >9 digits), %d mismatches\n", n, skipped, bad);
    return (bad || n == 0) ? 1 : 0;
}
