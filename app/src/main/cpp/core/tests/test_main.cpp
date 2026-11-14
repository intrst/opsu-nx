#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include "opsu/OpsuConstants.h"
#include "opsu/beatmap/LRUCache.h"
#include "opsu/logging/DefaultLogger.h"
#include "opsu/logging/StubLogger.h"
#include "opsu/objects/curves/Bezier2.h"
#include "opsu/objects/curves/CentripetalCatmullRom.h"
#include "opsu/replay/LifeFrame.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

struct EvictCache : LRUCache<int, int> {
    using LRUCache::LRUCache;
    int evictedKey = -1;
    void eldestRemoved(const int& k, int&) override { evictedKey = k; }
};

int main() {
    // Vec2f
    Vec2f v(3, 4);
    CHECK(near(v.len(), 5));
    CHECK(v.cpy().normalize() == Vec2f(0.6f, 0.8f));
    CHECK(Vec2f(1, 0).nor() == Vec2f(0, 1));
    CHECK(v.toString() == "(3.000, 4.000)");

    // Bezier2: 2 points = straight line, endpoints exact, midpoint at 0.5
    Bezier2 line({Vec2f(0, 0), Vec2f(100, 0)});
    CHECK(near(line.pointAt(0).x, 0) && near(line.pointAt(1).x, 100));
    CHECK(near(line.pointAt(0.5f).x, 50));
    CHECK(near(line.totalDistance(), 100));
    CHECK(line.getCurvesCount() == 100 / 4 + 2);

    // Bezier2: quadratic, midpoint = (P0 + 2*P1 + P2) / 4
    Bezier2 quad({Vec2f(0, 0), Vec2f(50, 100), Vec2f(100, 0)});
    CHECK(near(quad.pointAt(0.5f).x, 50) && near(quad.pointAt(0.5f).y, 50));

    // CentripetalCatmullRom passes through its 2nd and 3rd control points
    CentripetalCatmullRom cr({Vec2f(0, 0), Vec2f(10, 0), Vec2f(20, 10), Vec2f(30, 10)});
    CHECK(near(cr.pointAt(0).x, 10) && near(cr.pointAt(0).y, 0));
    CHECK(near(cr.pointAt(1).x, 20) && near(cr.pointAt(1).y, 10));
    bool threw = false;
    try { CentripetalCatmullRom bad({Vec2f(), Vec2f()}); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    // LRUCache
    EvictCache c(2);
    c.put(1, 10); c.put(2, 20);
    CHECK(c.get(1) && *c.get(1) == 10);   // touch 1 -> 2 becomes eldest
    c.put(3, 30);
    CHECK(c.evictedKey == 2 && !c.contains(2) && c.contains(1) && c.contains(3));
    CHECK(c.size() == 2);

    // LifeFrame
    CHECK(LifeFrame(1500, 0.756f).toString() == "(1500, 0.76)");
    // Java's %.Nf rounds ties half-up (printf gives 0.12 / 0.062)
    CHECK(LifeFrame(1, 0.125f).toString() == "(1, 0.13)");
    CHECK(Vec2f(0.0625f, 0.125f).toString() == "(0.063, 0.125)");

    // OpsuConstants
    CHECK(OpsuConstants::urlEncode("a b&c/ü") == "a+b%26c%2F%C3%BC");
    CHECK(OpsuConstants::getChangelogURI("0.16.1 beta") ==
          "https://github.com/fluddokt/opsu/releases/tag/0.16.1+beta");

    // Loggers
    std::ostringstream log;
    DefaultLogger::logStream = &log;
    DefaultLogger dl;
    dl.setSaveToFile(true);
    dl.info("T", "hello");
    CHECK(log.str().find(" T: hello\n") != std::string::npos);
    StubLogger::get().info("T", "stub ok");

    std::printf(failures ? "%d FAILED\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
