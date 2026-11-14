#pragma once
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "opsu/beatmap/BeatmapSetNode.h"

namespace opsu {

/**
 * PARTIAL port (batches 32, 33, 34, 35) of itdelatrisu.opsu.beatmap.BeatmapSetList: the node lists, `reset`, `init`, `addSongGroup`, counts,
 * `getBaseNode` / `getRandomNode` / `getNode`, `expand` / `unexpand` (32) and `search` / `applySearch` / `SearchSnapshot` /
 * `snapshotForSearch` / `isCurrentQuery` (33), the MD5 hash table and `getBeatmapFromHash` (34), `deleteSongGroup` / `deleteSong` over
 * the `DeleteHooks` seam (35). The static search half is BeatmapSearch (batch 30).
 * NOT ported yet (see the WIP table): the Java static `create` / `destroy` / `get` singleton (construct the object where needed), and the REAL
 * implementations behind `DeleteHooks` (BeatmapDB, MusicController, BeatmapWatchService, Utils.deleteToTrash: none exist in C++ yet).
 *
 * List aliasing (Java semantics, restored in batch 33): `parsedNodes`, `groupNodes` and `nodes` are three ArrayList REFERENCES. With the
 * group ALL, `BeatmapGroup.filter` returns its argument, so all three are ONE list (a node added by `addSongGroup` is in `size()` at
 * once and `init()` sorts `parsedNodes` too); FAVORITE / RECENT give a new `groupNodes`; an empty `search` query sets `nodes = groupNodes`
 * again, any other query makes `nodes` a new list. Here they are `shared_ptr<vector<BeatmapSetNode*>>` so the identity is observable
 * (`SearchSnapshot` keeps its owner alive, so a recycled address can never look like the same list).
 *
 * Ownership: the list owns every node. Java nodes live as long as someone references them; here the base nodes (`parsedNodes`) live
 * as long as the list, and the per-beatmap nodes made by `expand` are owned by the list until `RETIRED_EXPANSIONS` newer expansions
 * have happened. A pointer to an expanded node that is older than that (the song menu keeps at most a current and a previous one)
 * is dangling; Java would have kept it alive. `prev` / `next` of every node are raw pointers into these.
 *
 * Seams: BeatmapSet needs the current mods (`addSongGroup(beatmaps, current)`); `Math.random()` is `setRandomSource` (default: a
 * `std::mt19937_64` uniform in [0, 1)); the sort order and the group are the globals of BeatmapSortOrder / BeatmapGroup, like Java.
 *
 * Java quirks kept: `getNode(node, shift)` with a NEGATIVE shift never moves (the loop is `i < shift`), so it returns `node`;
 * `init()` clears the expansion without re-linking; `expand` of a set with no beatmaps returns null but still marks `index` expanded.
 */
class BeatmapSetList {
public:
    /** How many finished expansions stay alive (see above). */
    static constexpr std::size_t RETIRED_EXPANSIONS = 16;

    BeatmapSetList();

    /** Resets the fields (`nodes = groupNodes = current group's filter(parsedNodes)`, no expansion). Erases no parsed node. */
    void reset();

    /** Number of elements in the (group-filtered) list. */
    int size() const { return static_cast<int>(nodes_->size()); }

    /** Adds a set made of `beatmaps`; returns its (list-owned) node. @throws std::out_of_range for an empty vector (Java: beatmaps.get(0)) */
    BeatmapSetNode* addSongGroup(std::vector<Beatmap> beatmaps, const Beatmap::ModParams& current);

    /** Number of parsed beatmaps / beatmap sets. */
    int getMapCount() const { return mapCount_; }
    int getMapSetCount() const { return static_cast<int>(parsedNodes_->size()); }
    /** Beatmap set ID lookup (BeatmapSetList.containsBeatmapSetID; IDs <= 0 are never stored). */
    bool containsBeatmapSetID(int id) const { return msidDb_.count(id) != 0; }

    /** The node at an index, disregarding expansions; nullptr if out of range. */
    BeatmapSetNode* getBaseNode(int index) const;
    /** A random base node (`next` if it is the expanded one); nullptr for an empty list. */
    BeatmapSetNode* getRandomNode();
    /** `shift` nodes forward (+); see the quirk above for a negative shift. nullptr if the chain ends first. */
    BeatmapSetNode* getNode(BeatmapSetNode* node, int shift) const;

    /**
     * BeatmapSetList.getBeatmapFromHash: the beatmap whose MD5 hash was added by `addSongGroup`, or nullptr (Java null) if none (an empty hash is
     * never stored, so "" gives nullptr like Java's get(null)). The pointer is into the owning BeatmapSet, so it stays valid until that set is
     * re-sorted (`sortByDifficulty` moves the values) or has a beatmap removed; use it right away, do not keep it. Java kept a Beatmap
     * reference that survives sorts; here the table stores (set, file path) and finds the beatmap by path, so it always reaches the right one.
     */
    const Beatmap* getBeatmapFromHash(const std::string& beatmapHash) const;

    /**
     * The side effects of `deleteSongGroup` / `deleteSong` that live outside this class (batch 35). Every member is optional: an EMPTY function
     * does nothing (and the "is X active" queries answer false). **With the defaults nothing is deleted from disk** (`deleteToTrash` is empty): the
     * lists are updated and the methods return true, i.e. the headless mode used by the tests until the real BeatmapDB / MusicController /
     * file layer exist. Wire all of them before using this in the app.
     */
    struct DeleteHooks {
        /** `MusicController.trackExists() || MusicController.isTrackLoading()`. */
        std::function<bool()> trackActive;
        /** `MusicController.getBeatmap().audioFilename` of the playing beatmap ("" = null); only called when `trackActive()` is true. */
        std::function<std::string()> currentAudioFilename;
        /** `MusicController.reset(); Utils.gc(true);`: stop the track and release its file (Java needs the gc call before the files can be deleted). */
        std::function<void()> resetTrack;
        /** `BeatmapDB.delete(dirName)`: drop the cache entry of a whole beatmap directory (`File.getName()` of the directory). */
        std::function<void(const std::string& dirName)> dbDeleteDir;
        /** `BeatmapDB.delete(dirName, fileName)`: drop the cache entry of one beatmap file. */
        std::function<void(const std::string& dirName, const std::string& fileName)> dbDeleteFile;
        /** `Options.isWatchServiceEnabled() && BeatmapWatchService.get() != null`. */
        std::function<bool()> watchServiceActive;
        /** `ws.pause()` / `ws.resume()` around the deletion (only called when `watchServiceActive()` was true). */
        std::function<void()> pauseWatchService, resumeWatchService;
        /** `Utils.deleteToTrash(file)` on the full path of a directory (group) or file (song). A `java::IOException` is reported (`Download::reportError`) and swallowed like Java. */
        std::function<void(const std::string& path)> deleteToTrash;
    };
    /** Installs the hooks used by the delete methods (default: all empty). */
    void setDeleteHooks(DeleteHooks hooks) { hooks_ = std::move(hooks); }

    /**
     * BeatmapSetList.deleteSongGroup: removes the node's set from the three lists, the beatmap-set-ID set and the hash table, fixes
     * the base-node links / indices / expansion, stops the track if it plays this set's audio, drops the BeatmapDB entry and deletes the directory
     * (through the hooks), in Java's order. Returns false only for nullptr. Quirks kept: `nodes.remove(index)` then `parsedNodes.remove(eCur)` /
     * `groupNodes.remove(eCur)` (with the lists aliased the later two are no-ops); `MSIDdb.remove` and `beatmapHashDB.remove` go by KEY (a second
     * set with the same ID / hash loses its entry too); an IOException from `deleteToTrash` is reported, not thrown (the result is still true).
     * The node itself stays valid (the list keeps its memory) but is no longer in any list.
     * Java NullPointerException / IndexOutOfBounds -> std::logic_error / std::out_of_range, thrown at the same step (after the lists changed, as in Java).
     */
    bool deleteSongGroup(BeatmapSetNode* node);
    /**
     * BeatmapSetList.deleteSong: `node` must be an EXPANDED per-beatmap node (else false). The last beatmap of a set deletes the whole group instead.
     * Otherwise it shifts the later nodes' `beatmapIndex`, removes the beatmap from its set, erases its hash entry, re-links the node out of the chain,
     * drops the BeatmapDB entry and deletes the file (through the hooks). Java quirk kept: `expandedStart` / `expandedEnd` are NOT updated when the deleted node was
     * the first / last of the expansion (harmless for `unexpand`, which relinks from the base nodes; a later `deleteSongGroup` of the expanded group
     * then skips the index update of the last node, exactly like Java).
     */
    bool deleteSong(BeatmapSetNode* node);

    /** Diagnostics: number of entries in the MD5 table (the delete methods must erase theirs; a stale entry would also keep its set alive). */
    std::size_t beatmapHashCount() const { return beatmapHashDb_.size(); }

    int getExpandedIndex() const { return expandedIndex_; }
    BeatmapSetNode* expandedStartNode() const { return expandedStart_; }
    BeatmapSetNode* expandedEndNode() const { return expandedEnd_; }

    /** Undoes the previous expansion, then replaces the base node at `index` with one node per beatmap. Returns the first new node (nullptr if none / bad index). */
    BeatmapSetNode* expand(int index);

    /** Sorts the list by BeatmapSortOrder::current() (stable), clears the expansion and re-creates all links / indices. */
    void init();

    /** Java `SearchSnapshot`: a copy of the group's nodes (to filter on another thread) + what is needed to check it is still valid. */
    class SearchSnapshot {
    public:
        /** The copied nodes (filtered by the background thread). */
        const std::vector<BeatmapSetNode*>& source() const { return source_; }
    private:
        friend class BeatmapSetList;
        explicit SearchSnapshot(std::shared_ptr<std::vector<BeatmapSetNode*>> owner)
            : owner_(std::move(owner)), source_(*owner_), size_(owner_->size()) {}
        std::shared_ptr<std::vector<BeatmapSetNode*>> owner_;  // the `groupNodes` list the copy was taken from (identity check)
        std::vector<BeatmapSetNode*> source_;
        std::size_t size_;
    };

    /** True if the (normalized) query is the one the current `nodes` list was built from (`lastQuery != null && equals`). */
    bool isCurrentQuery(const std::string& normalizedQuery) const { return lastQuery_ && *lastQuery_ == normalizedQuery; }
    /** Copies the current group's nodes so they can be filtered elsewhere (BeatmapSearch `filterNodes`). */
    SearchSnapshot snapshotForSearch() const { return SearchSnapshot(groupNodes_); }
    /**
     * Installs the result of a background filter. Returns false (and changes nothing) if `groupNodes` is no longer the list the snapshot
     * was taken from, or its size changed (group switch / reset, removals). `result` must come from a finished (not cancelled) filter.
     */
    bool applySearch(const std::string& normalizedQuery, const SearchSnapshot& snapshot, std::vector<BeatmapSetNode*> result);
    /**
     * BeatmapSetList.search: normalizes `query` (nullopt = Java null -> false), returns false if it equals the previous query, else
     * `nodes` = `groupNodes` (empty query) or a new filtered list, `lastQuery` = the query, true. Touches neither the expansion nor the links
     * (Java leaves `expandedIndex` pointing at the old list; callers re-`init()` / `expand`).
     */
    bool search(const std::optional<std::string>& query);

    /** Math.random() replacement; must return a value in [0, 1). nullptr restores the default. */
    void setRandomSource(std::function<double()> source);

    /** Diagnostics: how many finished expansions are being kept alive (<= RETIRED_EXPANSIONS). */
    std::size_t retiredExpansions() const { return retired_.size(); }
    /** Test / diagnostics: the current list (search result, or the group's list) in order. */
    const std::vector<BeatmapSetNode*>& nodes() const { return *nodes_; }
    /** Test / diagnostics: the group-filtered list (what `search` filters). */
    const std::vector<BeatmapSetNode*>& groupNodes() const { return *groupNodes_; }
    /** Test / diagnostics: Java reference identity of the three lists. */
    bool nodesIsGroupNodes() const { return nodes_ == groupNodes_; }
    bool groupNodesIsParsedNodes() const { return groupNodes_ == parsedNodes_; }

private:
    void unexpand();
    void retireExpansion();

    using NodeList = std::vector<BeatmapSetNode*>;
    std::vector<std::unique_ptr<BeatmapSetNode>> ownedNodes_;  // owns every base node (the Java GC)
    std::shared_ptr<NodeList> parsedNodes_ = std::make_shared<NodeList>();  // Java `parsedNodes`
    std::shared_ptr<NodeList> groupNodes_, nodes_;  // Java `groupNodes` / `nodes`; see the aliasing note above
    struct HashEntry { std::shared_ptr<BeatmapSet> set; std::string file; };  // see getBeatmapFromHash
    std::unordered_map<std::string, HashEntry> beatmapHashDb_;  // Java `beatmapHashDB`: MD5 -> beatmap (a later put overwrites)
    std::optional<std::string> lastQuery_;  // Java `lastQuery` (null = nullopt)
    std::set<int> msidDb_;
    int mapCount_ = 0;
    int expandedIndex_ = -1;
    BeatmapSetNode *expandedStart_ = nullptr, *expandedEnd_ = nullptr;
    std::vector<std::unique_ptr<BeatmapSetNode>> expanded_;
    std::deque<std::vector<std::unique_ptr<BeatmapSetNode>>> retired_;
    std::function<double()> random_;
    DeleteHooks hooks_;
};

}  // namespace opsu
