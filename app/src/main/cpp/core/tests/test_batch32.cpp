// Tests for batch 32 (BeatmapSetList core: node lists, init, expand / unexpand, navigation). Behavioural; no JDK here.
#include <cstdio>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapGroup.h"
#include "opsu/beatmap/BeatmapSetList.h"
#include "opsu/beatmap/BeatmapSortOrder.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);
static std::vector<Beatmap> mkMaps(const std::string& title, int diffs, int msid, bool lastPlayed = false) {
    std::vector<Beatmap> v;
    for (int i = 0; i < diffs; i++) {
        Beatmap b("/x/" + title + std::to_string(i) + ".osu");
        b.title = title; b.artist = "art"; b.creator = "c"; b.version = "d" + std::to_string(i);
        b.starRating = 1 + i; b.beatmapSetID = msid; b.favorite = (title == "Fav");
        if (lastPlayed) b.lastPlayed = 1000;
        v.push_back(b);
    }
    return v;
}
// walk next links from the first node
static std::vector<std::string> walk(BeatmapSetNode* n) {
    std::vector<std::string> out; int guard = 0;
    for (; n && guard < 100; n = n->next, guard++)
        out.push_back(n->getBeatmapSet().get(0).title + (n->beatmapIndex >= 0 ? "#" + std::to_string(n->beatmapIndex) : ""));
    return out;
}
static std::vector<std::string> walkBack(BeatmapSetNode* n) {
    std::vector<std::string> out; int guard = 0;
    for (; n && guard < 100; n = n->prev, guard++)
        out.push_back(n->getBeatmapSet().get(0).title + (n->beatmapIndex >= 0 ? "#" + std::to_string(n->beatmapIndex) : ""));
    return out;
}
using V = std::vector<std::string>;

int main() {
    set(BeatmapSortOrder::TITLE);
    setGroup(BeatmapGroup::ALL);

    BeatmapSetList list;
    CHECK(list.size() == 0 && list.getMapCount() == 0 && list.getMapSetCount() == 0);
    CHECK(list.getBaseNode(0) == nullptr && list.getRandomNode() == nullptr && list.expand(0) == nullptr);
    list.init();  // empty: no-op

    // add (out of title order on purpose)
    BeatmapSetNode* c = list.addSongGroup(mkMaps("C", 3, 300), NOMOD);
    BeatmapSetNode* a = list.addSongGroup(mkMaps("A", 1, 100), NOMOD);
    BeatmapSetNode* b = list.addSongGroup(mkMaps("B", 2, 0), NOMOD);   // set id 0 is not stored
    BeatmapSetNode* d = list.addSongGroup(mkMaps("D", 2, -5), NOMOD);  // nor are negative ones
    CHECK(list.getMapCount() == 8 && list.getMapSetCount() == 4);
    CHECK(list.containsBeatmapSetID(300) && list.containsBeatmapSetID(100));
    CHECK(!list.containsBeatmapSetID(0) && !list.containsBeatmapSetID(-5) && !list.containsBeatmapSetID(7));
    // batch 33 correction: with the group ALL, Java's parsedNodes / groupNodes / nodes are ONE list, so the new nodes are in size() at once
    // (batch 32 wrongly had size() == 0 until reset(); init() without reset() then did nothing). Detailed in tests/test_batch33.cpp.
    CHECK(list.size() == 4);
    bool threw = false; try { list.addSongGroup({}, NOMOD); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);

    list.reset();
    CHECK(list.size() == 4 && list.getExpandedIndex() == -1);
    list.init();  // sorted by title, links and indices
    CHECK(list.getBaseNode(0) == a && list.getBaseNode(1) == b && list.getBaseNode(2) == c && list.getBaseNode(3) == d);
    CHECK(list.getBaseNode(-1) == nullptr && list.getBaseNode(4) == nullptr);
    CHECK(a->index == 0 && b->index == 1 && c->index == 2 && d->index == 3);
    CHECK(a->prev == nullptr && a->next == b && b->prev == a && d->next == nullptr && d->prev == c);
    CHECK(walk(a) == (V{"A", "B", "C", "D"}));

    // sort order is the global one: ARTIST ties keep insertion order (stable), then re-init
    set(BeatmapSortOrder::PLAYS); list.init();
    set(BeatmapSortOrder::TITLE); list.init();
    CHECK(list.getBaseNode(0) == a);

    // getNode: forward, past the end, the negative-shift quirk
    CHECK(list.getNode(a, 0) == a && list.getNode(a, 2) == c && list.getNode(a, 3) == d && list.getNode(a, 4) == nullptr);
    CHECK(list.getNode(c, -1) == c && list.getNode(c, -5) == c);   // Java never moves for a negative shift
    CHECK(list.getNode(nullptr, 3) == nullptr);

    // expand the 3rd node (C, 3 difficulties)
    BeatmapSetNode* first = list.expand(2);
    CHECK(first != nullptr && first != c && first->beatmapIndex == 0 && first->index == 2);
    CHECK(list.getExpandedIndex() == 2 && list.expandedStartNode() == first);
    CHECK(walk(a) == (V{"A", "B", "C#0", "C#1", "C#2", "D"}));
    CHECK(walkBack(d) == (V{"D", "C#2", "C#1", "C#0", "B", "A"}));
    CHECK(list.expandedEndNode() == first->next->next && list.expandedEndNode()->beatmapIndex == 2);
    CHECK(first->getBeatmapSet().get(0).version == "d0" && &first->getBeatmapSet() == &c->getBeatmapSet());
    CHECK(list.getBaseNode(2) == c);  // the base list is untouched
    CHECK(first->next->index == 2 && first->next->beatmapIndex == 1);
    // getNode through the expansion
    CHECK(list.getNode(b, 2) == first->next && list.getNode(b, 4) == d && list.getNode(b, 5) == nullptr);

    // random node: never the expanded group node itself (index == expandedIndex gives .next)
    list.setRandomSource([] { return 0.6; });  // (int)(0.6 * 4) = 2 -> the expanded base node c -> c->next (the first new node after unlinking? see below)
    BeatmapSetNode* rnd = list.getRandomNode();
    CHECK(rnd == c->next);
    list.setRandomSource([] { return 0.0; }); CHECK(list.getRandomNode() == a);
    list.setRandomSource([] { return 0.999; }); CHECK(list.getRandomNode() == d);
    list.setRandomSource(nullptr);
    for (int i = 0; i < 50; i++) CHECK(list.getRandomNode() != nullptr);

    // expand another: the first expansion is undone (group node restored)
    BeatmapSetNode* f2 = list.expand(1);
    CHECK(f2 && f2->beatmapIndex == 0 && list.getExpandedIndex() == 1);
    CHECK(walk(a) == (V{"A", "B#0", "B#1", "C", "D"}));
    CHECK(walkBack(d) == (V{"D", "C", "B#1", "B#0", "A"}));
    CHECK(c->index == 2 && b->index == 1);
    // expanding the first / last node
    list.expand(0);
    CHECK(walk(list.expandedStartNode()) == (V{"A#0", "B", "C", "D"}) && list.expandedStartNode()->prev == nullptr);
    list.expand(3);
    CHECK(walk(a) == (V{"A", "B", "C", "D#0", "D#1"}) && list.expandedEndNode()->next == nullptr);
    CHECK(walkBack(list.expandedEndNode()) == (V{"D#1", "D#0", "C", "B", "A"}));
    // bad index: previous expansion is undone, nothing expanded
    CHECK(list.expand(9) == nullptr && list.getExpandedIndex() == -1);
    CHECK(walk(a) == (V{"A", "B", "C", "D"}));
    CHECK(list.expand(-1) == nullptr);
    // many expansions keep working (retired generations are released)
    for (int i = 0; i < 100; i++) list.expand(i % 4);
    CHECK(list.getExpandedIndex() == 3 && walk(a).size() == 5);
    CHECK(list.retiredExpansions() == BeatmapSetList::RETIRED_EXPANSIONS);
    // init() while expanded clears the expansion and re-links the base nodes
    list.init();
    CHECK(list.getExpandedIndex() == -1 && list.expandedStartNode() == nullptr && walk(a) == (V{"A", "B", "C", "D"}));

    // init() resets the first node's prev, whatever it was
    {
        d->getBeatmapSet().get(0).title = "0D";  // D now sorts first
        list.init();
        CHECK(list.getBaseNode(0) == d && d->prev == nullptr && d->next == a && d->index == 0 && a->prev == d && c->next == nullptr);
        d->getBeatmapSet().get(0).title = "D";
        list.init();
        CHECK(list.getBaseNode(0) == a && a->prev == nullptr && d->next == nullptr);
    }

    // a set with 1 difficulty expands to one node
    list.expand(0);
    CHECK(walk(list.expandedStartNode()) == (V{"A#0", "B", "C", "D"}) && a->next == list.expandedStartNode());  // the hidden group node still points in
    list.init();

    // ties keep insertion order (stable), also for lists longer than 16 (libstdc++ std::sort is insertion sort below that)
    {
        BeatmapSetList big;
        std::vector<BeatmapSetNode*> added;
        for (int i = 0; i < 60; i++) added.push_back(big.addSongGroup(mkMaps("same", 1, 1000 + i), NOMOD));
        big.reset(); big.init();
        bool ok = big.size() == 60;
        for (int i = 0; i < 60 && ok; i++) ok = big.getBaseNode(i) == added[i] && added[i]->index == i;
        CHECK(ok);
    }

    // reset() uses the current group
    setGroup(BeatmapGroup::FAVORITE);
    list.reset();
    CHECK(list.size() == 0);
    list.addSongGroup(mkMaps("Fav", 1, 400), NOMOD);
    list.reset();
    CHECK(list.size() == 1 && list.getMapSetCount() == 5);
    list.init();
    CHECK(list.getBaseNode(0)->getBeatmapSet().get(0).title == "Fav" && list.getBaseNode(0)->next == nullptr);
    setGroup(BeatmapGroup::ALL);
    list.reset();
    CHECK(list.size() == 5);

    // message set ID is read from the set's own (sorted) first beatmap
    {
        BeatmapSetList l2;
        std::vector<Beatmap> v = mkMaps("Z", 2, 0);
        v[0].starRating = 9; v[1].starRating = 1; v[1].beatmapSetID = 777;   // after sorting, v[1] (easier) is first
        l2.addSongGroup(v, NOMOD);
        CHECK(l2.containsBeatmapSetID(777));
        std::vector<Beatmap> w = mkMaps("Y", 2, 0);
        w[0].starRating = 1; w[0].beatmapSetID = 0; w[1].starRating = 9; w[1].beatmapSetID = 888;
        l2.addSongGroup(w, NOMOD);
        CHECK(!l2.containsBeatmapSetID(888));
    }

    if (failures == 0) std::printf("batch32 (BeatmapSetList core): all tests passed\n");
    return failures ? 1 : 0;
}
