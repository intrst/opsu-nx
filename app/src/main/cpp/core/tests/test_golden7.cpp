// Seventh golden suite: checks util/JavaCompat's doubleToString / parseDouble against tests/golden/expected7.txt, produced by the
// real JDK (gen7.sh). One-sided: the file holds the inputs (bit patterns / strings) and the JDK's answers.
// usage: opsu_golden7 [expected7.txt]
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include "opsu/util/JavaCompat.h"

using namespace opsu;

static std::string hx(double d) {
    uint64_t u;
    if (d != d) u = 0x7ff8000000000000ull; else std::memcpy(&u, &d, 8);
    char b[24]; std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u)); return b;
}

int main(int argc, char** argv) {
    std::string path = argc > 1 ? argv[1] : "tests/golden/expected7.txt";
    std::ifstream f(path);
    if (!f) { std::printf("cannot open %s\n", path.c_str()); return 2; }
    std::string ln;
    int n = 0, bad = 0;
    while (std::getline(f, ln)) {
        size_t a = ln.find('|'), b = ln.rfind('|');
        if (a == std::string::npos || a == b) continue;
        std::string kind = ln.substr(0, a), mid = ln.substr(a + 1, b - a - 1), rest = ln.substr(b + 1);
        n++;
        std::string got;
        if (kind == "DTS") {
            uint64_t u = std::stoull(mid, nullptr, 16);
            double d; std::memcpy(&d, &u, 8);
            got = java::doubleToString(d);
        } else {
            try { got = hx(java::parseDouble(mid)); } catch (const java::NumberFormatException&) { got = "NFE"; }
        }
        if (got != rest) {
            if (bad++ < 15) std::printf("MISMATCH %s |%s|\n  jdk: %s\n  c++: %s\n", kind.c_str(), mid.c_str(), rest.c_str(), got.c_str());
        }
    }
    std::printf(bad ? "golden7: %d of %d cases differ\n" : "golden7: all %d cases match\n", bad ? bad : n, n);
    return bad ? 1 : 0;
}
