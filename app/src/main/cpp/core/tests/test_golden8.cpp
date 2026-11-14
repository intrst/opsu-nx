// Eighth golden suite: checks util/JavaCompat's urlEncode against tests/golden/expected8.txt, produced by the real JDK (gen8.sh).
// One-sided: each line holds the input (UTF-8 hex) and URLEncoder.encode(s, "UTF-8").
// usage: opsu_golden8 [expected8.txt]
#include <cstdio>
#include <fstream>
#include <string>
#include "opsu/util/JavaCompat.h"

using namespace opsu;

static std::string unhex(const std::string& h) {
    std::string s;
    for (size_t i = 0; i + 1 < h.size(); i += 2) s += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
    return s;
}

int main(int argc, char** argv) {
    std::string path = argc > 1 ? argv[1] : "tests/golden/expected8.txt";
    std::ifstream f(path);
    if (!f) { std::printf("cannot open %s\n", path.c_str()); return 2; }
    std::string ln;
    int n = 0, bad = 0;
    while (std::getline(f, ln)) {
        size_t a = ln.find('|'), b = ln.find('|', a + 1);
        if (a == std::string::npos || b == std::string::npos || ln.compare(0, a, "UE") != 0) continue;
        n++;
        std::string in = unhex(ln.substr(a + 1, b - a - 1)), want = ln.substr(b + 1);
        std::string got = java::urlEncode(in);
        if (got != want) {
            if (++bad <= 10) std::printf("MISMATCH in=%s\n  want %s\n  got  %s\n", ln.substr(a + 1, b - a - 1).c_str(), want.c_str(), got.c_str());
        }
    }
    std::printf("golden8: %d cases, %d mismatches\n", n, bad);
    return (bad || n == 0) ? 1 : 0;
}
