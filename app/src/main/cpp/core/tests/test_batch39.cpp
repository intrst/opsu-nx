// Tests for batch 39: the OsuToPjsk converter (the JDK-compared behaviour lives in test_golden11; this file covers what a JVM
// cannot compare: stream errors, the MAX_KEYS guard, the C++ entry points and a few hand-computed charts), plus java::trim / splitAll.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <istream>
#include <iterator>
#include <sstream>
#include <streambuf>
#include <string>
#include <stdexcept>
#include <vector>
#include "opsu/fourresonance/OsuToPjsk.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;
using namespace opsu::fourresonance;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const std::string SIMPLE =
    "osu file format v14\n[General]\nMode: 3\n[Difficulty]\nCircleSize:4\nOverallDifficulty:8\n[TimingPoints]\n0,500,4,1,0,60,1,0\n[HitObjects]\n"
    "64,192,1000,1,0,0:0:0:0:\n192,192,1500,128,0,2000:0:0:0:0:\n320,192,2500,1,0,0:0:0:0:\n448,192,3000,5,0,0:0:0:0:\n";

/** A stream whose buffer fails after `okBytes` (like a Reader throwing IOException mid-file). */
struct FailingBuf : std::streambuf {
    std::string data; size_t pos = 0, okBytes;
    FailingBuf(std::string d, size_t ok) : data(std::move(d)), okBytes(ok) {}
    int_type underflow() override {
        if (pos >= okBytes) throw std::runtime_error("disk error");
        if (pos >= data.size()) return traits_type::eof();
        ch = data[pos++];
        setg(&ch, &ch, &ch + 1);
        return traits_type::to_int_type(ch);
    }
    char ch = 0;
};

int main() {
    // ---- java::trim / splitAll
    CHECK(java::trim("  a b \t\r\n") == "a b");
    CHECK(java::trim("\x01 x \x1f") == "x");
    CHECK(java::trim("\xC2\xA0x") == "\xC2\xA0x");          // U+00A0 is > U+0020: not trimmed
    CHECK(java::trim("   ").empty() && java::trim("").empty());
    CHECK((java::splitAll("a,b,,", ',') == std::vector<std::string>{"a", "b", "", ""}));
    CHECK((java::splitAll("", ',') == std::vector<std::string>{""}));
    CHECK((java::splitAll(",", ',') == std::vector<std::string>{"", ""}));
    CHECK((java::split("a,b,,", ',') == std::vector<std::string>{"a", "b"}));   // the old helper still drops trailing empties

    // ---- the simple chart, computed by hand: 4 keys, 120 BPM (500 ms/beat), no lead-in
    {
        auto c = OsuToPjsk::convertString(SIMPLE, PjskChart::TailMode::RELEASE, true);
        CHECK(c.has_value());
        CHECK(c->keyCount == 4);
        CHECK(c->overallDifficulty == 8.f);
        CHECK(c->offsetMs == 0.0);
        CHECK(c->bpmSegments.size() == 1);
        CHECK(c->bpmSegments[0].startMs == 0 && c->bpmSegments[0].beatLengthMs == 500 && c->bpmSegments[0].bpm == 120);
        CHECK(c->timeScale.size() == 1 && c->timeScale[0].timeScale == 1.0);
        CHECK(c->notes.size() == 4);
        // x = 64 -> column floor(64 * 4 / 512) = 0 ; 192 -> 1 ; 320 -> 2 ; 448 -> 3
        for (int i = 0; i < 4; i++) CHECK(c->notes[i].column == i);
        CHECK(c->notes[0].beat == 2.0 && c->notes[0].timeMs == 1000 && !c->notes[0].hold);
        CHECK(c->notes[1].hold && c->notes[1].endBeat == 4.0 && c->notes[1].endTimeMs == 2000);
        CHECK(c->notes[1].tail == PjskChart::TailMode::RELEASE);
        CHECK(c->notes[0].size == 1.f);                                     // up to 5 keys: 2 units wide (half-width 1)
        // tinged columns on 4K: 0 and 3 are critical
        CHECK(c->notes[0].critical && !c->notes[1].critical && !c->notes[2].critical && c->notes[3].critical);
        // the same without the flag
        auto d = OsuToPjsk::convertString(SIMPLE, PjskChart::TailMode::TRACE, false);
        CHECK(d && !d->notes[0].critical && !d->notes[3].critical && d->notes[1].tail == PjskChart::TailMode::TRACE);
    }

    // ---- default arguments: minHold 0, maxSimultaneous 2 (the Java overloads)
    {
        std::string chord = "[General]\nMode: 3\n[Difficulty]\nCircleSize:4\n[HitObjects]\n"
                            "64,0,1000,1,0,0\n192,0,1000,1,0,0\n320,0,1000,1,0,0\n448,0,1000,1,0,0\n";
        auto a = OsuToPjsk::convertString(chord, PjskChart::TailMode::RELEASE, false);
        auto b = OsuToPjsk::convertString(chord, PjskChart::TailMode::RELEASE, false, 0, OsuToPjsk::DEFAULT_MAX_SIMULTANEOUS);
        auto none = OsuToPjsk::convertString(chord, PjskChart::TailMode::RELEASE, false, 0, 0);
        CHECK(OsuToPjsk::DEFAULT_MAX_SIMULTANEOUS == 2);
        CHECK(a && b && none);
        CHECK(a->notes.size() == 2 && b->notes.size() == 2 && none->notes.size() == 4);
        // the two kept notes are the leftmost (column 0), then the one farthest from it (column 3)
        CHECK(a->notes[0].column == 0 && a->notes[1].column == 3);
    }

    // ---- convert(istream) and convertString agree; the stream is consumed
    {
        std::istringstream in(SIMPLE);
        auto a = OsuToPjsk::convert(in, PjskChart::TailMode::NONE, true, 100, 3);
        auto b = OsuToPjsk::convertString(SIMPLE, PjskChart::TailMode::NONE, true, 100, 3);
        CHECK(a && b && a->notes.size() == b->notes.size() && a->timeScale.size() == b->timeScale.size());
        CHECK(a->notes[1].beat == b->notes[1].beat && a->notes[1].tail == PjskChart::TailMode::NONE);
        CHECK(in.peek() == std::char_traits<char>::eof());   // consumed (the parser reads the buffer directly, so eofbit itself is not set)
    }

    // ---- not a usable chart -> nullopt (the details are in the golden suite)
    CHECK(!OsuToPjsk::convertString("", PjskChart::TailMode::RELEASE, true));
    CHECK(!OsuToPjsk::convertString("[General]\nMode: 0\n", PjskChart::TailMode::RELEASE, true));
    {
        std::string s = SIMPLE;
        s.replace(s.find("Mode: 3"), 7, "Mode: 1");
        CHECK(!OsuToPjsk::convertString(s, PjskChart::TailMode::RELEASE, true));
    }

    // ---- a stream that fails while reading throws java::IOException (Java: the Reader's IOException)
    {
        FailingBuf fb(SIMPLE, 30);
        std::istream in(&fb);
        bool threw = false;
        try { OsuToPjsk::convert(in, PjskChart::TailMode::RELEASE, true); } catch (const java::IOException&) { threw = true; }
        CHECK(threw);
        // a stream that already failed (an ifstream of a missing file) is an IOException too, not a silent "not a chart"
        std::ifstream missing("/nonexistent/dir/missing.osu");
        threw = false;
        try { OsuToPjsk::convert(missing, PjskChart::TailMode::RELEASE, true); } catch (const java::IOException&) { threw = true; }
        CHECK(threw);
        // ...while a stream that is merely at EOF is an empty file: no chart
        std::istringstream used(SIMPLE);
        std::string sink((std::istreambuf_iterator<char>(used)), std::istreambuf_iterator<char>());
        CHECK(!OsuToPjsk::convert(used, PjskChart::TailMode::RELEASE, true).has_value());
    }

    // ---- MAX_KEYS guard: Java would run out of memory allocating the per-column arrays
    {
        auto withKeys = [](const std::string& cs) {
            return "[General]\nMode: 3\n[Difficulty]\nCircleSize:" + cs + "\n[HitObjects]\n0,0,100,1,0,0\n511,0,200,1,0,0\n";
        };
        bool threw = false;
        try { OsuToPjsk::convertString(withKeys("1e12"), PjskChart::TailMode::RELEASE, true); } catch (const std::length_error&) { threw = true; }
        CHECK(threw);
        threw = false;
        try { OsuToPjsk::convertString(withKeys(std::to_string(OsuToPjsk::MAX_KEYS + 1)), PjskChart::TailMode::RELEASE, true); } catch (const std::length_error&) { threw = true; }
        CHECK(threw);
        auto ok = OsuToPjsk::convertString(withKeys(std::to_string(OsuToPjsk::MAX_KEYS)), PjskChart::TailMode::RELEASE, true);
        CHECK(ok && ok->keyCount == OsuToPjsk::MAX_KEYS && ok->notes.size() == 2);
        // NaN / negative / fractional circle sizes are NOT errors
        CHECK(!OsuToPjsk::convertString(withKeys("NaN"), PjskChart::TailMode::RELEASE, true));
        CHECK(!OsuToPjsk::convertString(withKeys("-3"), PjskChart::TailMode::RELEASE, true));
        auto frac = OsuToPjsk::convertString(withKeys("4.99"), PjskChart::TailMode::RELEASE, true);
        CHECK(frac && frac->keyCount == 4);
    }

    // ---- LaneLayout / columnToLaneSize edges
    {
        auto ls = OsuToPjsk::columnToLaneSize(4, 0);
        CHECK(ls[0] == -5.f && ls[1] == 1.f);               // 4K: positions 0, 3, 7, 10 on an 11-position grid, width 2
        CHECK(OsuToPjsk::columnToLaneSize(4, 3)[0] == 5.f);
        CHECK(OsuToPjsk::columnToLaneSize(1, 0)[0] == 0.f);  // a single key sits in the middle
        CHECK(OsuToPjsk::columnToLaneSize(7, 6)[1] == 0.5f); // 6+ keys: 1 unit wide
        bool oob = false, nas = false;
        try { OsuToPjsk::columnToLaneSize(4, 4); } catch (const std::out_of_range&) { oob = true; }
        try { OsuToPjsk::columnToLaneSize(-1, 0); } catch (const java::NegativeArraySizeException&) { nas = true; }
        CHECK(oob && nas);
        CHECK(OsuToPjsk::isTingedColumn(7, 5) && !OsuToPjsk::isTingedColumn(7, 6) && !OsuToPjsk::isTingedColumn(3, 0) && !OsuToPjsk::isTingedColumn(8, 1));
    }

    // ---- the BPM helpers still behave as in batch 7 (the converter reuses them)
    {
        std::vector<PjskChart::BpmSegment> segs{PjskChart::BpmSegment(0, 0, 500, 120), PjskChart::BpmSegment(1000, 2, 250, 240)};
        CHECK(OsuToPjsk::timeMsToBeat(1500, segs) == 4.0);
        CHECK(OsuToPjsk::beatToTimeMs(4.0, segs) == 1500.0);
        CHECK(OsuToPjsk::timeMsToBeat(700, {}) == 1.4 && OsuToPjsk::beatToTimeMs(2, {}) == 1000);
    }

    // ---- notes before the first red line: whole beats are added in front, so beat numbers stay >= 0
    {
        std::string s = SIMPLE;
        s.replace(s.find("0,500,4,1,0,60,1,0"), 18, "1250,500,4,1,0,60,1,0");   // first note at 1000 ms is before it
        auto c = OsuToPjsk::convertString(s, PjskChart::TailMode::RELEASE, false);
        CHECK(c && c->bpmSegments.size() == 2);
        CHECK(c->bpmSegments[0].beatAtStart == 0.0 && c->bpmSegments[0].startMs == 750.0);   // 1 beat earlier: floor(250 / 500) + 1 = 1
        CHECK(c->bpmSegments[1].beatAtStart == 1.0 && c->offsetMs == 750.0);
        CHECK(c->notes[0].beat == 0.5);                                                      // (1000 - 750) / 500 + beatAtStart 0
    }

    if (failures) { std::printf("batch39: %d FAILED\n", failures); return 1; }
    std::printf("batch39: all passed\n");
    return 0;
}
