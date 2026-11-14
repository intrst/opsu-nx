// Tests for batch 6 (BeatmapSortOrder, Integer2/Long2 compare helpers).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapSortOrder.h"
#include "opsu/util/JavaMath.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Beatmap mk(const char* title, const char* artist, const char* creator, int bpmMax, int endTime,
                  int64_t dateAdded, int playCount) {
    Beatmap b("x.osu");
    b.title = title; b.artist = artist; b.creator = creator;
    b.bpmMax = bpmMax; b.endTime = endTime; b.dateAdded = dateAdded; b.playCount = playCount;
    return b;
}
using Set = BeatmapSetBeatmaps;

int main() {
    // helpers keep Java's `i - j` semantics (magnitude + wrap), not -1/0/1
    CHECK(java::integer2Compare(5, 3) == 2);
    CHECK(java::integer2Compare(3, 5) == -2);
    CHECK(java::integer2Compare(std::numeric_limits<int>::max(), -1) == std::numeric_limits<int>::min());
    CHECK(java::integer2Compare(std::numeric_limits<int>::min(), 1) == std::numeric_limits<int>::max());
    CHECK(java::long2Compare(10, 4) == 6);
    CHECK(java::long2Compare(0x100000005LL, 0) == 5);          // (int) of the low 32 bits
    CHECK(java::long2Compare(0x100000000LL, 0) == 0);          // wraps to "equal" in Java too
    CHECK(java::long2Compare(INT64_MIN, 1) == -1);  // MIN-1 wraps to 0x7FFF...FFFF, low 32 bits = 0xFFFFFFFF

    // names / order
    CHECK(std::string(name(BeatmapSortOrder::DATE)) == "DATE");
    CHECK(std::string(getName(BeatmapSortOrder::DATE)) == "Date Added");
    CHECK(std::string(getName(BeatmapSortOrder::PLAYS)) == "Most Played");
    CHECK(static_cast<int>(BeatmapSortOrder::PLAYS) == (int)BeatmapSortOrder_COUNT - 1);

    Set a = {mk("apple", "Zed", "bob", 120, 1000, 5, 2), mk("apple2", "Zed", "bob", 200, 5000, 50, 3)};
    Set b = {mk("Banana", "alpha", "Carl", 150, 4000, 20, 10)};

    // string orders: case-insensitive, first beatmap of each set only
    CHECK(compare(BeatmapSortOrder::TITLE, a, b) < 0);           // "apple" < "Banana"
    CHECK(compare(BeatmapSortOrder::TITLE, b, a) > 0);
    CHECK(compare(BeatmapSortOrder::ARTIST, a, b) > 0);          // "Zed" > "alpha"
    CHECK(compare(BeatmapSortOrder::CREATOR, a, b) < 0);         // "bob" < "Carl"
    CHECK(compare(BeatmapSortOrder::TITLE, a, a) == 0);
    // BPM uses get(0).bpmMax only (120 vs 150), not the set's max
    CHECK(compare(BeatmapSortOrder::BPM, a, b) == -30);
    // LENGTH: longest beatmap of each set (5000 vs 4000)
    CHECK(compare(BeatmapSortOrder::LENGTH, a, b) == 1000);
    // DATE: latest dateAdded of each set (50 vs 20)
    CHECK(compare(BeatmapSortOrder::DATE, a, b) == 30);
    // PLAYS: sums (5 vs 10)
    CHECK(compare(BeatmapSortOrder::PLAYS, a, b) == -5);
    // endTime defaults to -1: the max starts at 0 like Java, so an unset set compares as 0
    Set unset = {mk("u", "u", "u", 0, -1, 0, 0)};
    CHECK(compare(BeatmapSortOrder::LENGTH, unset, unset) == 0);
    CHECK(compare(BeatmapSortOrder::LENGTH, unset, b) == -4000);
    // PLAYS sum wraps like Java int
    Set big = {mk("t", "t", "t", 0, 0, 0, std::numeric_limits<int>::max()), mk("t", "t", "t", 0, 0, 0, 1)};
    Set zero = {mk("t", "t", "t", 0, 0, 0, 0)};
    CHECK(compare(BeatmapSortOrder::PLAYS, big, zero) == std::numeric_limits<int>::min());

    // empty set -> IndexOutOfBounds equivalent for orders that call get(0); sum/max orders are fine
    Set empty;
    bool threw = false;
    try { compare(BeatmapSortOrder::TITLE, empty, a); } catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
    CHECK(compare(BeatmapSortOrder::LENGTH, empty, empty) == 0);
    CHECK(compare(BeatmapSortOrder::PLAYS, empty, zero) == 0);

    // stable sort through less()
    {
        std::vector<Set> sets = {b, a, unset};
        std::stable_sort(sets.begin(), sets.end(),
                         [](const Set& x, const Set& y) { return less(BeatmapSortOrder::LENGTH, x, y); });
        CHECK(sets[0][0].title == "u" && sets[1][0].title == "Banana" && sets[2][0].title == "apple");
    }

    // saved()/current()/set()
    resetForTesting();
    CHECK(saved() == BeatmapSortOrder::TITLE);                   // no provider
    setSavedSortOrderProvider([] { return std::optional<std::string>("PLAYS"); });
    CHECK(saved() == BeatmapSortOrder::PLAYS);
    CHECK(current() == BeatmapSortOrder::PLAYS);                 // lazily loaded
    setSavedSortOrderProvider([] { return std::optional<std::string>("DATE"); });
    CHECK(current() == BeatmapSortOrder::PLAYS);                 // loaded once, not re-read
    set(BeatmapSortOrder::BPM);
    CHECK(current() == BeatmapSortOrder::BPM);
    CHECK(savedFromName(std::nullopt) == BeatmapSortOrder::TITLE);
    CHECK(savedFromName(std::string("nonsense")) == BeatmapSortOrder::TITLE);
    CHECK(savedFromName(std::string("title")) == BeatmapSortOrder::TITLE);   // names are case-sensitive: falls back
    CHECK(savedFromName(std::string("LENGTH")) == BeatmapSortOrder::LENGTH);
    resetForTesting();
    set(BeatmapSortOrder::ARTIST);                               // set() before current() suppresses the load
    setSavedSortOrderProvider([] { return std::optional<std::string>("DATE"); });
    CHECK(current() == BeatmapSortOrder::ARTIST);

    if (failures == 0) std::printf("batch6: all tests passed\n");
    return failures ? 1 : 0;
}
