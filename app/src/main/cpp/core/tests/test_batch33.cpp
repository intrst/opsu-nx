// Tests for batch 33 (BeatmapSetList: search / applySearch / SearchSnapshot / isCurrentQuery + the nodes / groupNodes / parsedNodes
// aliasing that Java has and batch 32 got wrong). Behavioural; no JDK here.
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapGroup.h"
#include "opsu/beatmap/BeatmapSearch.h"
#include "opsu/beatmap/BeatmapSetList.h"
#include "opsu/beatmap/BeatmapSortOrder.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);
// `diffs` beatmaps with star ratings first, first+1, ...; the artist is "zz" so a short term only hits the title.
static std::vector<Beatmap> mk(const std::string& title, int diffs, int msid, int firstStar = 1, bool fav = false) {
    std::vector<Beatmap> v;
    for (int i = 0; i < diffs; i++) {
        Beatmap b("/x/" + title + std::to_string(i) + ".osu");
        b.title = title; b.artist = "zz"; b.creator = "c"; b.version = "d" + std::to_string(i);
        b.starRating = static_cast<float>(firstStar + i); b.beatmapSetID = msid; b.favorite = fav;
        v.push_back(b);
    }
    return v;
}
static std::string name(BeatmapSetNode* n) { return n ? n->getBeatmapSet().get(0).title : "<null>"; }
static std::vector<std::string> names(const std::vector<BeatmapSetNode*>& v) {
    std::vector<std::string> out;
    for (BeatmapSetNode* n : v) out.push_back(name(n));
    return out;
}
using V = std::vector<std::string>;
static std::optional<std::string> Q(const char* s) { return std::string(s); }

// Cat(1-3), Apple(1), Banana(1-2), Dog(4-5), inserted in this (unsorted) order. Favourites: Cat, Banana.
static void fill(BeatmapSetList& l, BeatmapSetNode** out = nullptr) {
    BeatmapSetNode* n[4];
    n[0] = l.addSongGroup(mk("Cat", 3, 10, 1, true), NOMOD);
    n[1] = l.addSongGroup(mk("Apple", 1, 20, 1, false), NOMOD);
    n[2] = l.addSongGroup(mk("Banana", 2, 30, 1, true), NOMOD);
    n[3] = l.addSongGroup(mk("Dog", 2, 40, 4, false), NOMOD);
    if (out) for (int i = 0; i < 4; i++) out[i] = n[i];
}

int main() {
    set(BeatmapSortOrder::TITLE);
    setGroup(BeatmapGroup::ALL);

    // ---- aliasing, group ALL: parsedNodes == groupNodes == nodes (Java: BeatmapGroup.ALL.filter returns its argument) ----
    {
        BeatmapSetList l;
        CHECK(l.nodesIsGroupNodes() && l.groupNodesIsParsedNodes() && l.size() == 0);
        BeatmapSetNode* n[4];
        fill(l, n);
        CHECK(l.size() == 4);  // in the list at once, no reset() needed
        CHECK(names(l.nodes()) == (V{"Cat", "Apple", "Banana", "Dog"}));
        CHECK(l.nodesIsGroupNodes() && l.groupNodesIsParsedNodes());
        l.init();  // init() right after the adds (the real load order) must sort and link
        CHECK(names(l.nodes()) == (V{"Apple", "Banana", "Cat", "Dog"}));
        CHECK(n[1]->prev == nullptr && n[1]->next == n[2] && n[3]->next == nullptr && n[0]->index == 2);
        CHECK(l.getMapSetCount() == 4 && l.getMapCount() == 8);
        l.reset();  // reset() keeps the same (already sorted) list instance
        CHECK(names(l.nodes()) == (V{"Apple", "Banana", "Cat", "Dog"}) && l.groupNodesIsParsedNodes());
    }

    // ---- aliasing, group FAVORITE: groupNodes is a NEW list, a later add reaches parsedNodes only ----
    {
        setGroup(BeatmapGroup::FAVORITE);
        BeatmapSetList l;
        fill(l);
        l.reset();
        CHECK(l.size() == 2 && names(l.nodes()) == (V{"Cat", "Banana"}));
        CHECK(l.nodesIsGroupNodes() && !l.groupNodesIsParsedNodes());
        l.addSongGroup(mk("Fig", 1, 50, 1, true), NOMOD);
        CHECK(l.getMapSetCount() == 5 && l.size() == 2);  // not in the group list until reset()
        l.reset();
        CHECK(l.size() == 3 && names(l.nodes()) == (V{"Cat", "Banana", "Fig"}));
        l.init();  // sorts only the group list
        CHECK(names(l.nodes()) == (V{"Banana", "Cat", "Fig"}));
        // search only looks inside the current group: Apple matches "a" but is not a favourite, Dog is the only 4-star set
        CHECK(l.search(Q("a")) && names(l.nodes()) == (V{"Banana", "Cat"}));
        CHECK(l.search(Q("star>=4")) && l.size() == 0);
        CHECK(l.search(Q("")) && names(l.nodes()) == (V{"Banana", "Cat", "Fig"}) && l.nodesIsGroupNodes());
        setGroup(BeatmapGroup::ALL);
    }

    // ---- search ----
    {
        BeatmapSetList l;
        BeatmapSetNode* n[4];
        fill(l, n);
        CHECK(!l.search(std::nullopt));  // Java null
        CHECK(!l.isCurrentQuery("") && !l.isCurrentQuery("an"));  // lastQuery is still null

        CHECK(l.search(Q("an")) && names(l.nodes()) == (V{"Banana"}) && l.size() == 1);
        CHECK(!l.nodesIsGroupNodes());
        CHECK(names(l.groupNodes()) == (V{"Cat", "Apple", "Banana", "Dog"}));  // untouched
        CHECK(l.isCurrentQuery("an") && !l.isCurrentQuery("AN") && !l.isCurrentQuery("") && !l.isCurrentQuery("a"));
        CHECK(!l.search(Q("an")));  // same query
        CHECK(!l.search(Q("  AN \t")));  // normalized first: trimmed + lower case
        CHECK(names(l.nodes()) == (V{"Banana"}));

        CHECK(l.search(Q("a")) && names(l.nodes()) == (V{"Cat", "Apple", "Banana"}));  // source order, not sorted
        CHECK(l.search(Q("star>=4")) && names(l.nodes()) == (V{"Dog"}));
        CHECK(l.search(Q("a star>=2")) && names(l.nodes()) == (V{"Cat", "Banana"}));  // text term, then the condition
        CHECK(l.search(Q("zzzz")) && l.size() == 0);
        CHECK(l.getBaseNode(0) == nullptr && l.getRandomNode() == nullptr);

        // an empty query goes back to the SAME list object, not to a copy
        CHECK(l.search(Q("   ")) && l.nodesIsGroupNodes() && l.size() == 4);
        CHECK(l.isCurrentQuery("") && !l.search(Q("")));
        CHECK(l.search(Q("an")) && l.search(Q("")) && l.nodesIsGroupNodes());

        // reset() forgets the query: the same text searches again; and the very first empty query is a real (list-restoring) search
        l.reset();
        CHECK(!l.isCurrentQuery("") && !l.isCurrentQuery("an"));
        CHECK(l.search(Q("")) && l.nodesIsGroupNodes());
        l.reset();
        CHECK(l.search(Q("an")) && l.search(Q("")) == true);
        l.reset();
        CHECK(l.search(Q("an")));
        l.reset();
        CHECK(l.search(Q("an")) && l.size() == 1);

        // searching an empty list
        BeatmapSetList e;
        CHECK(e.search(Q("x")) && e.size() == 0 && !e.search(Q("x")));
    }

    // ---- init() after a search sorts (and links) only the result list; groupNodes keeps its order ----
    {
        BeatmapSetList l;
        BeatmapSetNode* n[4];
        fill(l, n);  // Cat, Apple, Banana, Dog (no init)
        CHECK(l.search(Q("a star>=2")) && names(l.nodes()) == (V{"Cat", "Banana"}));
        l.init();
        CHECK(names(l.nodes()) == (V{"Banana", "Cat"}));
        CHECK(names(l.groupNodes()) == (V{"Cat", "Apple", "Banana", "Dog"}));  // the Java quirk
        CHECK(n[2]->index == 0 && n[0]->index == 1 && n[2]->prev == nullptr && n[2]->next == n[0] && n[0]->prev == n[2] && n[0]->next == nullptr);
        CHECK(n[1]->next == nullptr && n[1]->prev == nullptr);  // Apple was never linked
        CHECK(l.getBaseNode(0) == n[2] && l.getBaseNode(1) == n[0] && l.getBaseNode(2) == nullptr);
    }

    // ---- search does not touch the expansion; the shrunken list makes unexpand()'s `expandedIndex >= size()` guard reachable ----
    {
        BeatmapSetList l;
        fill(l);
        l.init();  // Apple, Banana, Cat, Dog
        BeatmapSetNode* start = l.expand(2);  // Cat: 3 difficulties
        CHECK(start && l.getExpandedIndex() == 2);
        CHECK(l.search(Q("star>=4")) && names(l.nodes()) == (V{"Dog"}));
        CHECK(l.getExpandedIndex() == 2 && l.expandedStartNode() == start);  // untouched, now stale (Java does the same)
        BeatmapSetNode* e = l.expand(0);  // unexpand() sees index 2 >= size 1 and returns; must not dereference a missing node
        CHECK(e && name(e) == "Dog" && l.getExpandedIndex() == 0 && e->beatmapIndex == 0);
    }

    // ---- SearchSnapshot / applySearch ----
    {
        // group FAVORITE: reset() makes a NEW groupNodes, which invalidates a snapshot of the old one
        setGroup(BeatmapGroup::FAVORITE);
        BeatmapSetList l;
        fill(l);
        l.reset();  // Cat, Banana
        BeatmapSetList::SearchSnapshot snap = l.snapshotForSearch();
        CHECK(names(snap.source()) == (V{"Cat", "Banana"}));
        auto res = filterNodes("an", snap.source(), [](BeatmapSetNode* n) -> const BeatmapSet& { return n->getBeatmapSet(); });
        CHECK(res && names(*res) == (V{"Banana"}));
        CHECK(l.applySearch("an", snap, *res));
        CHECK(names(l.nodes()) == (V{"Banana"}) && l.isCurrentQuery("an") && !l.nodesIsGroupNodes());
        CHECK(names(l.groupNodes()) == (V{"Cat", "Banana"}));
        CHECK(!l.search(Q("an")));  // applySearch set lastQuery

        // the same snapshot can be applied again (nothing changed), e.g. a second result for it
        CHECK(l.applySearch("a", snap, {}) && l.size() == 0 && l.isCurrentQuery("a"));

        l.reset();  // new group list: the old snapshot is stale
        CHECK(!l.applySearch("an", snap, *res));
        CHECK(l.size() == 2 && !l.isCurrentQuery("an") && !l.isCurrentQuery("a") && l.nodesIsGroupNodes());  // nothing changed
        snap = l.snapshotForSearch();  // a fresh one works again (and the old list, freed by reset(), was kept alive by the snapshot: ASan)
        CHECK(l.applySearch("x", snap, {}) && l.size() == 0);
        setGroup(BeatmapGroup::ALL);
    }
    {
        // group ALL: groupNodes is parsedNodes, reset() keeps the identity, but a changed SIZE invalidates the snapshot
        BeatmapSetList l;
        fill(l);
        BeatmapSetList::SearchSnapshot snap = l.snapshotForSearch();
        l.reset();
        CHECK(l.applySearch("q", snap, {}) && l.size() == 0 && l.isCurrentQuery("q"));  // identity unchanged, size unchanged
        l.reset();
        BeatmapSetList::SearchSnapshot snap2 = l.snapshotForSearch();
        l.addSongGroup(mk("Eel", 1, 60), NOMOD);  // size 4 -> 5
        CHECK(!l.applySearch("q", snap2, {}) && l.size() == 5 && !l.isCurrentQuery("q"));
        // the snapshot is a COPY: later changes to the list do not show in it
        CHECK(snap2.source().size() == 4);
    }

    // ---- searching keeps working with the expanded / retired node machinery of batch 32 ----
    {
        BeatmapSetList l;
        fill(l);
        l.init();
        for (int i = 0; i < 40; i++) {  // enough expansions to retire some
            l.expand(i % 4);
            if (i % 7 == 0) { l.search(Q("a")); l.search(Q("")); }
        }
        CHECK(l.retiredExpansions() <= BeatmapSetList::RETIRED_EXPANSIONS);
    }

    setGroup(BeatmapGroup::ALL);
    if (failures == 0) std::printf("batch33 (BeatmapSetList search / snapshot / aliasing): all tests passed\n");
    return failures ? 1 : 0;
}
