// Tests for batch 35 (BeatmapSetList::deleteSongGroup / deleteSong over the DeleteHooks seam). Behavioural: the expected values were
// worked out from BeatmapSetList.java by hand (statement by statement) because the Java class needs the whole engine to run.
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapGroup.h"
#include "opsu/beatmap/BeatmapSetList.h"
#include "opsu/downloads/Download.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define THROWS(expr, T) do { bool t_ = false; try { (void)(expr); } catch (const T&) { t_ = true; } if (!t_) { std::printf("FAIL %s:%d  expected %s: %s\n", __FILE__, __LINE__, #T, #expr); failures++; } } while (0)

using V = std::vector<std::string>;
static const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);

// set i (0 = "A") has sizes[i] beatmaps: /songs/dirA/A0.osu, md5 "hA0", beatmapSetID 100+i, audio /songs/dirA/audio.mp3
static void fill(BeatmapSetList& l, const std::vector<int>& sizes) {
    for (std::size_t i = 0; i < sizes.size(); i++) {
        std::string t(1, static_cast<char>('A' + i)), dir = "/songs/dir" + t;
        std::vector<Beatmap> v;
        for (int j = 0; j < sizes[i]; j++) {
            Beatmap b(dir + "/" + t + std::to_string(j) + ".osu");
            b.title = t; b.version = "v" + std::to_string(j); b.md5Hash = "h" + t + std::to_string(j);
            b.beatmapSetID = 100 + static_cast<int>(i); b.starRating = static_cast<float>(j + 1); b.audioFilename = dir + "/audio.mp3";
            v.push_back(b);
        }
        l.addSongGroup(v, NOMOD);
    }
    l.init();
}
static std::string label(const BeatmapSetNode* n) { return n->getBeatmapSet().get(0).title + (n->beatmapIndex >= 0 ? ":" + std::to_string(n->beatmapIndex) : ""); }
// the linked chain, forward from `start` (follows `next`), and backward from its end (follows `prev`)
static V fwd(const BeatmapSetNode* n) { V v; for (; n; n = n->next) v.push_back(label(n)); return v; }
static V bwd(const BeatmapSetNode* n) { V v; if (!n) return v; while (n->next) n = n->next; for (; n; n = n->prev) v.push_back(label(n)); V r(v.rbegin(), v.rend()); return r; }
static V names(const std::vector<BeatmapSetNode*>& l) { V v; for (auto* n : l) v.push_back(label(n)); return v; }
static BeatmapSetNode* head(const BeatmapSetList& l) { return l.getExpandedIndex() == 0 ? l.expandedStartNode() : l.getBaseNode(0); }
// forward chain == reverse of the backward chain, and every base node's index matches its place
static bool consistent(const BeatmapSetList& l) {
    if (l.size() == 0) return true;
    if (fwd(head(l)) != bwd(head(l))) return false;
    for (int i = 0; i < l.size(); i++) if (l.getBaseNode(i)->index != i) return false;
    return true;
}

struct Rec {  // records the hook calls in order
    V log; bool track = false, watch = false; std::string audio; bool throwIo = false, throwOther = false;
    BeatmapSetList::DeleteHooks hooks() {
        BeatmapSetList::DeleteHooks h;
        h.trackActive = [this] { log.push_back("trackActive"); return track; };
        h.currentAudioFilename = [this] { log.push_back("audio?"); return audio; };
        h.resetTrack = [this] { log.push_back("reset"); };
        h.dbDeleteDir = [this](const std::string& d) { log.push_back("dbdir:" + d); };
        h.dbDeleteFile = [this](const std::string& d, const std::string& f) { log.push_back("dbfile:" + d + "|" + f); };
        h.watchServiceActive = [this] { return watch; };
        h.pauseWatchService = [this] { log.push_back("pause"); };
        h.resumeWatchService = [this] { log.push_back("resume"); };
        h.deleteToTrash = [this](const std::string& p) {
            log.push_back("trash:" + p);
            if (throwIo) throw java::IOException("disk on fire");
            if (throwOther) throw std::runtime_error("other");
        };
        return h;
    }
};

int main() {
    setGroup(BeatmapGroup::ALL);

    // ---- null, and the plain case: group ALL, nothing expanded (nodes == groupNodes == parsedNodes)
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {2, 1, 3, 1});  // A B C D, 7 maps
        CHECK(!l.deleteSongGroup(nullptr) && r.log.empty() && l.size() == 4);
        BeatmapSetNode* b = l.getBaseNode(1);
        CHECK(l.nodesIsGroupNodes() && l.groupNodesIsParsedNodes());
        CHECK(l.deleteSongGroup(b));
        CHECK(l.size() == 3 && l.getMapSetCount() == 3 && l.getMapCount() == 6);          // the aliased list shrinks once, not three times
        CHECK(names(l.nodes()) == (V{"A", "C", "D"}));
        CHECK(fwd(head(l)) == (V{"A", "C", "D"}) && consistent(l));
        CHECK(l.getBaseNode(1)->index == 1 && l.getBaseNode(2)->index == 2);               // indices of the later nodes shifted down
        CHECK(!l.containsBeatmapSetID(101) && l.containsBeatmapSetID(100) && l.containsBeatmapSetID(102));
        CHECK(l.getBeatmapFromHash("hB0") == nullptr && l.getBeatmapFromHash("hC1") != nullptr);
        CHECK(l.beatmapHashCount() == 6);                                                    // 7 hashes minus B's one: the entry itself is gone
        CHECK(r.log == (V{"trackActive", "dbdir:dirB", "trash:/songs/dirB"}));
        // first and last
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        CHECK(fwd(head(l)) == (V{"C", "D"}) && consistent(l) && l.getBaseNode(0)->prev == nullptr);
        CHECK(l.deleteSongGroup(l.getBaseNode(1)));
        CHECK(fwd(head(l)) == (V{"C"}) && consistent(l) && l.getBaseNode(0)->next == nullptr);
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));                                        // the only node: the list becomes empty
        CHECK(l.size() == 0 && l.getMapCount() == 0 && l.getMapSetCount() == 0);
        for (int i = 100; i < 104; i++) CHECK(!l.containsBeatmapSetID(i));
    }

    // ---- an expanded group: deleting a group before / after / next to it / the expanded one itself
    {
        auto setup = [](BeatmapSetList& l, Rec& r) { l.setDeleteHooks(r.hooks()); fill(l, {1, 1, 3, 1, 1}); l.expand(2); };  // A B [C:0 C:1 C:2] D E
        {   // delete a group BEFORE the expanded one (not adjacent): A
            BeatmapSetList l; Rec r; setup(l, r);
            CHECK(fwd(l.getBaseNode(0)) == (V{"A", "B", "C:0", "C:1", "C:2", "D", "E"}));
            CHECK(l.deleteSongGroup(l.getBaseNode(0)));
            CHECK(l.getExpandedIndex() == 1);
            CHECK(fwd(l.getBaseNode(0)) == (V{"B", "C:0", "C:1", "C:2", "D", "E"}) && bwd(l.getBaseNode(0)) == fwd(l.getBaseNode(0)));
            for (auto* n = l.expandedStartNode(); n != l.expandedEndNode()->next; n = n->next) CHECK(n->index == 1);  // every expanded node moved with it
            CHECK(l.getBaseNode(1)->index == 1 && l.getBaseNode(3)->index == 3);
        }
        {   // delete the group right BEFORE the expanded one: B (eNext is the expanded base node)
            BeatmapSetList l; Rec r; setup(l, r);
            CHECK(l.deleteSongGroup(l.getBaseNode(1)));
            CHECK(fwd(l.getBaseNode(0)) == (V{"A", "C:0", "C:1", "C:2", "D", "E"}) && bwd(l.getBaseNode(0)) == fwd(l.getBaseNode(0)));
            CHECK(l.getExpandedIndex() == 1 && l.expandedStartNode()->prev == l.getBaseNode(0) && l.getBaseNode(0)->next == l.expandedStartNode());
        }
        {   // delete the group right AFTER the expanded one: D (ePrev is the expanded base node)
            BeatmapSetList l; Rec r; setup(l, r);
            CHECK(l.deleteSongGroup(l.getBaseNode(3)));
            CHECK(fwd(l.getBaseNode(0)) == (V{"A", "B", "C:0", "C:1", "C:2", "E"}) && bwd(l.getBaseNode(0)) == fwd(l.getBaseNode(0)));
            CHECK(l.getExpandedIndex() == 2 && l.expandedEndNode()->next == l.getBaseNode(3) && l.getBaseNode(3)->prev == l.expandedEndNode());
            CHECK(l.getBaseNode(3)->index == 3 && label(l.getBaseNode(3)) == "E");
        }
        {   // delete a group AFTER, not adjacent: E
            BeatmapSetList l; Rec r; setup(l, r);
            CHECK(l.deleteSongGroup(l.getBaseNode(4)));
            CHECK(fwd(l.getBaseNode(0)) == (V{"A", "B", "C:0", "C:1", "C:2", "D"}) && l.getExpandedIndex() == 2);
        }
        {   // delete the expanded group itself: the expansion is cleared and the neighbours link to each other
            BeatmapSetList l; Rec r; setup(l, r);
            CHECK(l.deleteSongGroup(l.expandedStartNode()->next));                            // any node of the group (here C:1)
            CHECK(l.getExpandedIndex() == -1 && l.expandedStartNode() == nullptr && l.expandedEndNode() == nullptr);
            CHECK(fwd(l.getBaseNode(0)) == (V{"A", "B", "D", "E"}) && consistent(l));
            CHECK(r.log == (V{"trackActive", "dbdir:dirC", "trash:/songs/dirC"}));
            CHECK(l.getMapCount() == 4 && !l.containsBeatmapSetID(102));
        }
    }

    // ---- non-aliased lists: with an active search `nodes` is its own list; the three removals are different
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {1, 1, 1, 1});  // titles A B C D
        CHECK(l.search(std::optional<std::string>("d")));                                    // query semantics are batch 33's; here only the lists matter
        CHECK(!l.nodesIsGroupNodes() && l.size() == 1);
        l.init();                                                                            // re-link the result list (callers do this after a search)
        BeatmapSetNode* d = l.getBaseNode(0);
        CHECK(label(d) == "D" && l.groupNodes().size() == 4 && l.getMapSetCount() == 4);
        CHECK(l.deleteSongGroup(d));
        CHECK(l.size() == 0 && l.getMapSetCount() == 3 && l.groupNodes().size() == 3);       // removed from the result, from parsedNodes AND groupNodes
        CHECK(names(l.groupNodes()) == (V{"A", "B", "C"}) && names(l.nodes()).empty());
        CHECK(l.search(std::optional<std::string>("")) && l.nodesIsGroupNodes() && l.size() == 3);
    }
    {   // group FAVORITE: groupNodes is a new list, parsedNodes keeps the node until it is removed explicitly
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {1, 1, 1});
        for (auto* n : l.groupNodes()) if (label(n) != "B") n->getBeatmapSet().get(0).favorite = true;
        setGroup(BeatmapGroup::FAVORITE);
        l.reset();                                                                           // nodes = groupNodes = filter(parsedNodes) = A, C
        l.init();
        CHECK(!l.groupNodesIsParsedNodes() && l.size() == 2 && l.getMapSetCount() == 3);
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));                                          // A
        CHECK(l.size() == 1 && l.groupNodes().size() == 1 && l.getMapSetCount() == 2);
        setGroup(BeatmapGroup::ALL);
    }

    {   // group FAVORITE + an active search: nodes, groupNodes and parsedNodes are THREE lists, all three removals matter
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {1, 1, 1, 1});                                                               // A B C D
        for (auto* n : l.groupNodes()) if (label(n) != "B") n->getBeatmapSet().get(0).favorite = true;
        setGroup(BeatmapGroup::FAVORITE);
        l.reset(); l.init();                                                                 // groupNodes = A C D
        CHECK(l.search(std::optional<std::string>("c")) && l.size() == 1);                   // nodes = C only
        l.init();
        CHECK(!l.nodesIsGroupNodes() && !l.groupNodesIsParsedNodes() && l.groupNodes().size() == 3 && l.getMapSetCount() == 4);
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));                                          // C
        CHECK(l.size() == 0 && names(l.groupNodes()) == (V{"A", "D"}) && l.getMapSetCount() == 3);
        setGroup(BeatmapGroup::ALL);
    }

    // ---- deleteSong
    {
        auto setup = [](BeatmapSetList& l, Rec& r) { l.setDeleteHooks(r.hooks()); fill(l, {1, 4, 1}); };  // A, B (B0..B3), C
        {   // not expanded / base node / wrong group / null -> false, nothing touched
            BeatmapSetList l; Rec r; setup(l, r);
            CHECK(!l.deleteSong(nullptr) && !l.deleteSong(l.getBaseNode(1)));
            l.expand(1);
            BeatmapSetNode* a = l.getBaseNode(0);                                            // a plain base node of another group
            CHECK(!l.deleteSong(a) && !l.deleteSong(l.getBaseNode(2)) && r.log.empty() && l.getMapCount() == 6);
            l.expand(0);                                                                      // group A expanded: node B... is no longer expanded
        }
        {   // a middle beatmap
            BeatmapSetList l; Rec r; setup(l, r);
            BeatmapSetNode* first = l.expand(1);
            BeatmapSetNode* n1 = first->next;                                                // B:1
            CHECK(fwd(l.getBaseNode(0)) == (V{"A", "B:0", "B:1", "B:2", "B:3", "C"}));
            CHECK(l.deleteSong(n1));
            CHECK(l.getMapCount() == 5 && n1->getBeatmapSet().size() == 3);
            CHECK(l.getBeatmapFromHash("hB1") == nullptr && l.getBeatmapFromHash("hB0") != nullptr && l.getBeatmapFromHash("hB2") != nullptr);
            CHECK(l.beatmapHashCount() == 5);                                                // 6 hashes minus hB1: erased, not just unreachable
            CHECK(fwd(l.getBaseNode(0)) == (V{"A", "B:0", "B:1", "B:2", "C"}) && bwd(l.getBaseNode(0)) == fwd(l.getBaseNode(0)));  // later nodes' beatmapIndex fell by one
            CHECK(first->next->getBeatmapSet().get(static_cast<std::size_t>(first->next->beatmapIndex)).md5Hash == "hB2");     // B:1 now IS the old B2
            CHECK(r.log == (V{"dbfile:dirB|B1.osu", "trash:/songs/dirB/B1.osu"}));
        }
        {   // the FIRST beatmap: expandedStart is left pointing at the deleted node (Java quirk), harmless for unexpand
            BeatmapSetList l; Rec r; setup(l, r);
            BeatmapSetNode* first = l.expand(1);
            CHECK(l.deleteSong(first));
            CHECK(l.expandedStartNode() == first && fwd(l.getBaseNode(0)) == (V{"A", "B:0", "B:1", "B:2", "C"}));
            CHECK(l.getBaseNode(0)->next->prev == l.getBaseNode(0));
            // deleting a group BEFORE it now: Java walks `size` nodes from the stale start, so the LAST expanded node keeps its old index
            CHECK(l.deleteSongGroup(l.getBaseNode(0)));
            CHECK(l.getExpandedIndex() == 0);
            BeatmapSetNode* last = l.expandedEndNode();
            CHECK(last->index == 1);                                                          // quirk: should be 0, Java leaves it
            CHECK(l.expandedStartNode()->next->index == 0);                                   // the live ones were updated
            l.expand(0);                                                                      // unexpand relinks from the base nodes: still fine
            CHECK(l.getExpandedIndex() == 0 && consistent(l));
        }
        {   // the LAST beatmap: expandedEnd stale
            BeatmapSetList l; Rec r; setup(l, r);
            BeatmapSetNode* first = l.expand(1);
            BeatmapSetNode* lastN = l.expandedEndNode();
            CHECK(l.deleteSong(lastN));
            CHECK(l.expandedEndNode() == lastN && fwd(l.getBaseNode(0)) == (V{"A", "B:0", "B:1", "B:2", "C"}));
            CHECK(l.getBaseNode(0)->next->next->next->next == l.getBaseNode(2) && l.getBaseNode(2)->prev == first->next->next);
            CHECK(bwd(l.getBaseNode(0)) == fwd(l.getBaseNode(0)));
        }
        {   // the last beatmap of a set deletes the whole group
            BeatmapSetList l; Rec r; setup(l, r);
            BeatmapSetNode* c = l.expand(2);                                                 // C has one beatmap
            CHECK(l.deleteSong(c));
            CHECK(l.size() == 2 && l.getMapCount() == 5 && l.getExpandedIndex() == -1 && fwd(l.getBaseNode(0)) == (V{"A", "B"}));
            CHECK(r.log == (V{"trackActive", "dbdir:dirC", "trash:/songs/dirC"}));          // group path, not file path
        }
        {   // shrinking one beatmap at a time until the group goes
            BeatmapSetList l; Rec r; setup(l, r);
            l.expand(1);
            for (int k = 0; k < 3; k++) CHECK(l.deleteSong(l.expandedStartNode()) && l.getBaseNode(1) != nullptr);
            CHECK(l.getBaseNode(0)->getBeatmapSet().size() == 1 && l.getMapCount() == 3);
        }
    }

    // ---- Java quirks of the tables: removal BY KEY
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        Beatmap a("/songs/dirX/x.osu"), b("/songs/dirY/y.osu");
        a.title = "X"; a.md5Hash = "same"; a.beatmapSetID = 7; b.title = "Y"; b.md5Hash = "same"; b.beatmapSetID = 7;
        BeatmapSetNode* nx = l.addSongGroup({a}, NOMOD);
        l.addSongGroup({b}, NOMOD);
        l.init();
        CHECK(l.containsBeatmapSetID(7) && l.getBeatmapFromHash("same") != nullptr);
        CHECK(l.deleteSongGroup(nx));
        CHECK(!l.containsBeatmapSetID(7) && l.getBeatmapFromHash("same") == nullptr);        // Y still exists but lost both entries, like Java
        CHECK(l.getMapSetCount() == 1 && l.getMapCount() == 1);
    }

    // ---- an "expanded" group whose set has no beatmaps (expand marks it expanded but returns null): Java NullPointerExceptions
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {1, 1, 1});                                                                  // A B C
        l.getBaseNode(2)->getBeatmapSet().remove(0);                                         // C is empty now
        CHECK(l.expand(2) == nullptr && l.getExpandedIndex() == 2 && l.expandedStartNode() == nullptr);
        THROWS(l.deleteSongGroup(l.getBaseNode(0)), std::logic_error);                       // index shift of the expanded group needs expandedStartNode
        CHECK(l.size() == 2 && l.getExpandedIndex() == 1);                                   // thrown after the lists changed, like Java
        CHECK(r.log.empty());
    }
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {1, 1, 1});
        l.getBaseNode(1)->getBeatmapSet().remove(0);                                         // B is empty and "expanded"
        l.expand(1);
        CHECK(l.expandedEndNode() != nullptr);                                               // expand() leaves expandedEnd on the (detached) base node, only the start is null
        CHECK(l.deleteSongGroup(l.getBaseNode(2)) && l.size() == 2);                         // ePrev is the expanded node: `expandedEndNode.next = eNext` works, no exception
        THROWS(l.deleteSongGroup(l.getBaseNode(0)), std::logic_error);                       // eNext is the expanded node: `expandedStartNode.prev` is a NullPointerException
        CHECK(l.size() == 2);                                                                // it failed before any list changed
    }

    // ---- hooks: track, watch service, errors
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {1, 1, 1, 1, 1, 1, 1});
        r.track = true; r.audio = "/songs/dirA/audio.mp3";                                    // the playing track belongs to the deleted set
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        CHECK(r.log == (V{"trackActive", "audio?", "reset", "dbdir:dirA", "trash:/songs/dirA"}));
        r.log.clear(); r.audio = "/songs/dirOther/audio.mp3";                                 // another track: left alone
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        CHECK(r.log == (V{"trackActive", "audio?", "dbdir:dirB", "trash:/songs/dirB"}));
        r.log.clear(); r.audio = "";                                                          // no audio file name (Java null): left alone
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        CHECK(r.log == (V{"trackActive", "audio?", "dbdir:dirC", "trash:/songs/dirC"}));
        r.log.clear(); r.audio = "";                                                          // ... nor when the beatmap has none either (null.equals(null) is not a match)
        l.getBaseNode(0)->getBeatmapSet().get(0).audioFilename = "";
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        CHECK(r.log == (V{"trackActive", "audio?", "dbdir:dirD", "trash:/songs/dirD"}));
        r.log.clear(); r.track = false; r.watch = true;                                       // watch service paused around the deletion
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        CHECK(r.log == (V{"trackActive", "dbdir:dirE", "pause", "trash:/songs/dirE", "resume"}));
        // an IOException is reported (message, flag, exception) and swallowed; the watch service is resumed; result true
        std::string msg; bool rep = false; int calls = 0; std::string what;
        Download::setErrorHandler([&](const std::string& m, const std::exception* e, bool report) { msg = m; rep = report; calls++; what = e ? e->what() : "<null>"; });
        r.log.clear(); r.throwIo = true;
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        CHECK(calls == 1 && msg == "Could not delete song group." && rep && what == "disk on fire");
        CHECK(r.log == (V{"trackActive", "dbdir:dirF", "pause", "trash:/songs/dirF", "resume"}));
        // any other exception propagates; the watch service is NOT resumed (Java has no finally); the lists were already changed
        r.log.clear(); r.throwIo = false; r.throwOther = true;
        THROWS(l.deleteSongGroup(l.getBaseNode(0)), std::runtime_error);
        CHECK(r.log == (V{"trackActive", "dbdir:dirG", "pause", "trash:/songs/dirG"}) && l.size() == 0);
        Download::setErrorHandler({});
    }
    {   // the same for a single song: message, order, db entry by (dir, file)
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {3});
        r.watch = true; r.throwIo = true; std::string msg; int calls = 0; bool rep = false;
        Download::setErrorHandler([&](const std::string& m, const std::exception*, bool report) { msg = m; rep = report; calls++; });
        CHECK(l.deleteSong(l.expand(0)->next));
        CHECK(calls == 1 && msg == "Could not delete song." && rep);
        CHECK(r.log == (V{"dbfile:dirA|A1.osu", "pause", "trash:/songs/dirA/A1.osu", "resume"}));  // deleteSong never touches the music
        Download::setErrorHandler({});
    }

    // ---- default (empty) hooks: lists change, nothing else happens, no crash
    {
        BeatmapSetList l; fill(l, {2, 1});
        CHECK(l.deleteSong(l.expand(0)));
        CHECK(l.deleteSongGroup(l.getBaseNode(1)) && l.getMapCount() == 1 && l.size() == 1);
    }

    // ---- error cases that Java reports as exceptions, at the same step (after the lists changed)
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        fill(l, {1, 1});
        l.getBaseNode(0)->index = 9;                                                           // stale index (e.g. after a search): nodes.remove(9)
        THROWS(l.deleteSongGroup(l.getBaseNode(0)), std::out_of_range);
        CHECK(r.log.empty() && l.size() == 2);                                                 // nothing was removed or deleted
    }
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        Beatmap b("lonely.osu"); b.title = "Z"; b.md5Hash = "hz"; b.beatmapSetID = 5;           // a file with no parent directory
        BeatmapSetNode* n = l.addSongGroup({b}, NOMOD);
        l.init();
        THROWS(l.deleteSongGroup(n), std::logic_error);                                        // Java: NullPointerException at dir.getName(), after the lists changed
        CHECK(l.size() == 0 && l.getMapCount() == 0 && !l.containsBeatmapSetID(5) && l.getBeatmapFromHash("hz") == nullptr);
        CHECK(r.log == (V{"trackActive"}));
    }

    // ---- java.io.File path rules (normalisation, root, relative)
    {
        BeatmapSetList l; Rec r; l.setDeleteHooks(r.hooks());
        std::vector<std::string> files = {"/songs//d1//a.osu", "/a.osu", "rel/dir/b.osu", "/x/y/z/c.osu"};
        for (std::size_t i = 0; i < files.size(); i++) {
            Beatmap b(files[i]); b.title = std::string(1, static_cast<char>('A' + i)); b.beatmapSetID = 0;
            l.addSongGroup({b, b}, NOMOD);                                                      // two beatmaps: deleteSong must not delete the group
        }
        l.init();
        l.expand(0); CHECK(l.deleteSong(l.expandedStartNode()));
        l.expand(1); CHECK(l.deleteSong(l.expandedStartNode()));   // "/a.osu": parent is "/" whose name is ""
        l.expand(2); CHECK(l.deleteSong(l.expandedStartNode()));
        l.expand(3); CHECK(l.deleteSong(l.expandedStartNode()));
        CHECK(r.log == (V{"dbfile:d1|a.osu", "trash:/songs/d1/a.osu", "dbfile:|a.osu", "trash:/a.osu",
                          "dbfile:dir|b.osu", "trash:rel/dir/b.osu", "dbfile:z|c.osu", "trash:/x/y/z/c.osu"}));
    }

    if (failures == 0) std::printf("batch35 (BeatmapSetList deleteSongGroup / deleteSong): all tests passed\n");
    return failures ? 1 : 0;
}
