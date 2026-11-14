// Tests for batch 31 (BeatmapSetNode data + text half). Behavioural; no JDK here.
#include <cstdio>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapSetNode.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define CHECK_EQ(a, b) do { std::string _a = (a), _b = (b); if (_a != _b) { std::printf("FAIL %s:%d  %s\n  got: %s\n  exp: %s\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); failures++; } } while (0)

static const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);
static BeatmapSet mkSet() {
    std::vector<Beatmap> v;
    for (auto name : {"Easy", "Hard"}) {
        Beatmap b(std::string("/x/") + name + ".osu");
        b.title = "T"; b.artist = "A"; b.creator = "C"; b.version = name; b.starRating = name[0] == 'E' ? 1.0 : 4.0;
        b.endTime = 60000; b.bpmMin = b.bpmMax = 100;
        v.push_back(b);
    }
    return BeatmapSet(v, NOMOD);
}
// every character is 10 wide (so width == 10 * number of code points)
static int measures = 0;
static const TextMeasure M = [](const std::string& s) {
    measures++;
    float n = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++;
    return n * 10.f;
};

int main() {
    BeatmapSetNode node(mkSet());
    // defaults / constants
    CHECK(node.beatmapIndex == -1 && node.index == 0 && node.prev == nullptr && node.next == nullptr);
    CHECK(node.panelHover == 0 && node.rowHover == 0 && node.select == 0);
    CHECK(BeatmapSetNode::MAX_SPREAD_ICONS == 18 && BeatmapSetNode::ROW_HEIGHT_SCALE == 1.5f && BeatmapSetNode::CHIP_SCALE == 1.25f);
    CHECK(BeatmapSetNode::PANEL_BAR_TOP == 16.f / 127.f && BeatmapSetNode::PANEL_BAR_H == 100.f / 127.f);

    // selected beatmap
    CHECK(node.getSelectedBeatmap() == nullptr);
    node.beatmapIndex = 0; CHECK(node.getSelectedBeatmap() && node.getSelectedBeatmap()->version == "Easy");
    node.beatmapIndex = 1; CHECK(node.getSelectedBeatmap()->version == "Hard");
    node.beatmapIndex = 2; CHECK(node.getSelectedBeatmap() == nullptr);
    node.beatmapIndex = -5; CHECK(node.getSelectedBeatmap() == nullptr);
    node.beatmapIndex = 1; node.getSelectedBeatmap()->favorite = true;
    CHECK(node.getBeatmapSet().isFavorite());  // pointer into the set, not a copy

    // shared set
    {
        BeatmapSetNode other(node.beatmapSetPtr());
        CHECK(&other.getBeatmapSet() == &node.getBeatmapSet());
    }

    // getInfo / toString
    node.beatmapIndex = -1;
    CHECK(!node.getInfo(false, NOMOD).has_value());
    CHECK_EQ(node.toString(false), "A - T");
    node.beatmapIndex = 0;
    auto info = node.getInfo(false, NOMOD);
    CHECK(info && info->size() == 5 && (*info)[0] == "A - T [Easy]" && (*info)[4].find("Stars:1.00") != std::string::npos);
    CHECK_EQ(node.toString(false), "A - T [Easy]");
    node.beatmapIndex = 1; CHECK_EQ(node.toString(false), "A - T [Hard]");
    node.beatmapIndex = 2;
    bool threw = false; try { node.toString(false); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);
    threw = false; try { node.getInfo(false, NOMOD); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);
    node.beatmapIndex = -2;
    threw = false; try { node.toString(false); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);

    // truncate
    CHECK_EQ(BeatmapSetNode::truncate(M, "hello", 50), "hello");       // fits exactly
    CHECK_EQ(BeatmapSetNode::truncate(M, "hello", 49), "h...");        // "he..." is 50 > 49, "h..." is 40
    CHECK_EQ(BeatmapSetNode::truncate(M, "", 0), "");
    CHECK_EQ(BeatmapSetNode::truncate(M, "abcdef", 30), "a...");       // "a..." = 40 > 30, but the loop stops at len 1
    CHECK_EQ(BeatmapSetNode::truncate(M, "abcdef", 1000), "abcdef");
    CHECK_EQ(BeatmapSetNode::truncate(M, "x", -1), "x...");            // len stays 1
    CHECK_EQ(BeatmapSetNode::truncate(M, "", -1), "...");              // len 0: substring(0,0) + "..."
    // UTF-8: whole code points are cut
    CHECK_EQ(BeatmapSetNode::truncate(M, "\xE3\x82\xA2\xE3\x82\xA4\xE3\x82\xA6\xE3\x82\xA8\xE3\x82\xAA\xE3\x82\xAB", 50), "\xE3\x82\xA2\xE3\x82\xA4...");

    // 2-byte (e-acute) and 4-byte (emoji) code points are cut whole too
    CHECK_EQ(BeatmapSetNode::truncate(M, "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 35), "\xC3\xA9...");   // 4 chars: "é..." = 40 > 35, len stops at 1
    CHECK_EQ(BeatmapSetNode::truncate(M, "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 50), "\xC3\xA9\xC3\xA9...");
    CHECK_EQ(BeatmapSetNode::truncate(M, "\xF0\x9F\x98\x80\xF0\x9F\x98\x81\xF0\x9F\x98\x82\xF0\x9F\x98\x83\xF0\x9F\x98\x84\xF0\x9F\x98\x85", 50),
             "\xF0\x9F\x98\x80\xF0\x9F\x98\x81...");
    CHECK_EQ(BeatmapSetNode::truncate(M, "\xF0\x9F\x98\x80\xF0\x9F\x98\x81\xF0\x9F\x98\x82", 20), "\xF0\x9F\x98\x80...");  // stops at one whole emoji
    CHECK_EQ(BeatmapSetNode::truncate(M, "a\xC3", 5), "a...");   // broken sequence: counted as one unit, never read past the end

    // fit cache
    {
        TextFit c; int font1 = 1, font2 = 2;
        measures = 0;
        CHECK_EQ(BeatmapSetNode::fit(c, &font1, std::nullopt, "hello world", 60, M), "hel...");
        int first = measures; CHECK(first > 0);
        CHECK_EQ(BeatmapSetNode::fit(c, &font1, std::nullopt, "hello world", 60, M), "hel...");
        CHECK(measures == first);                                  // cached: no measuring
        CHECK_EQ(BeatmapSetNode::fit(c, &font1, std::nullopt, "hello world", 80, M), "hello...");   // width changed
        CHECK(measures > first); first = measures;
        BeatmapSetNode::fit(c, &font2, std::nullopt, "hello world", 80, M); CHECK(measures > first); first = measures;  // font changed
        BeatmapSetNode::fit(c, &font2, std::nullopt, "hello there", 80, M); CHECK(measures > first); first = measures;  // text changed
        // quirk: the prefix is not part of the cache key
        TextFit d;
        CHECK_EQ(BeatmapSetNode::fit(d, &font1, std::string(" by "), "abc", 1000, M), " by abc");
        CHECK_EQ(BeatmapSetNode::fit(d, &font1, std::nullopt, "abc", 1000, M), " by abc");
        CHECK_EQ(BeatmapSetNode::fit(d, &font1, std::nullopt, "abd", 1000, M), "abd");
        // prefix is included in the truncation
        TextFit e;
        CHECK_EQ(BeatmapSetNode::fit(e, &font1, std::string("by "), "abcdef", 60, M), "by ...");
    }

    // rating text
    {
        BeatmapSetNode n2(mkSet());
        CHECK_EQ(n2.ratingText(4.567), "4.57");
        CHECK_EQ(n2.ratingText(4.567), "4.57");
        CHECK_EQ(n2.ratingText(0.125), "0.13");   // %.2f is HALF_UP
        CHECK_EQ(n2.ratingText(-1), "-1.00");
        CHECK_EQ(n2.ratingText(5), "5.00");
    }

    if (failures == 0) std::printf("batch31 (BeatmapSetNode data/text): all tests passed\n");
    return failures ? 1 : 0;
}
