#pragma once
#include <array>
#include <cstddef>
#include <vector>
#include "opsu/beatmap/BeatmapSortOrder.h"  // BeatmapSetBeatmaps

namespace opsu {

class BeatmapSetNode;

/**
 * Beatmap groups (tabs of the song menu). PARTIAL port of itdelatrisu.opsu.beatmap.BeatmapGroup:
 * the filters and the tab geometry are ported; the tab sprite / `contains` / `draw` need the backend (WIP).
 *
 * Seams:
 * - BeatmapSetNode / BeatmapSet are not ported, so a filter works on the sets' beatmaps (like BeatmapSortOrder) and returns
 *   INDICES into the input list, in the order Java would return the nodes. The template overload `filter(g, nodes, getSet)`
 *   maps that back to nodes once BeatmapSetNode exists (`getSet(node)` -> `const BeatmapSetBeatmaps&`).
 * - RECENT returns the contents of a java.util.PriorityQueue in its INTERNAL HEAP ORDER (not sorted). The queue is
 *   reproduced with the JDK's exact siftUp / siftDown, so the order (including ties) is the same as Java's.
 * - **Java quirk kept**: the RECENT comparator is `Long2.compare(a, b)` = `(int)(a - b)`. Two "last played" times (ms) more
 *   than 2^31 ms (~24.8 days) apart overflow the cast and can compare the wrong way round, so for a library played over more
 *   than ~25 days the "Last Played" tab can show the wrong 20 sets. Fixing it means using a real long compare in
 *   BeatmapGroup.java and here (one line each); this port deliberately matches the Java as it is.
 * - The tab (MenuButton) is just its centre here. `init` takes the tab image size (`GameImage.MENU_TAB.getImage()`);
 *   `getTabLeft` uses the width given to the last `init` (Java re-reads the image, which init is re-run for anyway).
 *   Tab accessors throw std::logic_error before `init` (Java: NullPointerException).
 */
enum class BeatmapGroup : int { ALL, RECENT, FAVORITE };

constexpr std::size_t BeatmapGroup_COUNT = 3;

/** BeatmapGroup.values() in declaration order. */
constexpr std::array<BeatmapGroup, BeatmapGroup_COUNT> beatmapGroupValues() {
    return {BeatmapGroup::ALL, BeatmapGroup::RECENT, BeatmapGroup::FAVORITE};
}
/** BeatmapGroup.VALUES_REVERSED. */
constexpr std::array<BeatmapGroup, BeatmapGroup_COUNT> beatmapGroupValuesReversed() {
    return {BeatmapGroup::FAVORITE, BeatmapGroup::RECENT, BeatmapGroup::ALL};
}

/** Enum constant name (Java `name()`): "ALL", "RECENT", "FAVORITE". */
const char* name(BeatmapGroup g);
/** Display name (the Java private `name` field, drawn on the tab): "All Songs", "Last Played", "Favorites". */
const char* getName(BeatmapGroup g);
/** The ID of the group (tab position): 0, 1, 2. */
int getId(BeatmapGroup g);
/** The message shown when the list is empty, or nullptr (Java null) for ALL. */
const char* getEmptyMessage(BeatmapGroup g);

/** BeatmapGroup.current() (starts as ALL). Named currentGroup() because BeatmapSortOrder owns opsu::current(). */
BeatmapGroup currentGroup();
/** BeatmapGroup.set(group) (setGroup: BeatmapSortOrder owns opsu::set()). */
void setGroup(BeatmapGroup g);

/** Number of elements the RECENT group keeps. */
constexpr int RECENT_K = 20;

/**
 * BeatmapGroup.filter: the indices (into `sets`) of the nodes Java would return, in Java's order.
 * ALL: every index in order. RECENT: the top RECENT_K most recently played sets (unplayed = lastPlayed 0 skipped) in
 * PriorityQueue heap order. FAVORITE: every set with a favorite beatmap, in order.
 */
std::vector<std::size_t> filter(BeatmapGroup g, const std::vector<const BeatmapSetBeatmaps*>& sets);

/** filter() for a list of nodes of any type; `getSet(node)` returns the node's beatmaps. */
template <class Node, class GetSet>
std::vector<Node> filter(BeatmapGroup g, const std::vector<Node>& list, GetSet getSet) {
    std::vector<const BeatmapSetBeatmaps*> sets;
    sets.reserve(list.size());
    for (const Node& n : list) sets.push_back(&getSet(n));
    std::vector<Node> out;
    for (std::size_t i : filter(g, sets)) out.push_back(list[i]);
    return out;
}

/**
 * BeatmapGroup.filter(ArrayList<BeatmapSetNode>) for the real node type (batch 38): same result as the template above with
 * `getSet = node->getBeatmapSet().beatmaps()`. Pointers are returned as given (no copies); nodes must not be null.
 */
std::vector<BeatmapSetNode*> filter(BeatmapGroup g, const std::vector<BeatmapSetNode*>& list);

/** The "latest last played time" of a set (0 if none), as Java's private `lastPlayed(BeatmapSet)`. */
int64_t lastPlayed(const BeatmapSetBeatmaps& set);
/** BeatmapSet.isFavorite(): any beatmap is a favorite. */
bool isFavorite(const BeatmapSetBeatmaps& set);

/** BeatmapGroup.init(containerWidth, bottomY) for one group: lays the tab out (tab image is tabWidth x tabHeight). */
void init(BeatmapGroup g, int containerWidth, float bottomY, int tabWidth, int tabHeight);
/** Centre x / y of the group's tab (after init). */
float getTabCenterX(BeatmapGroup g);
float getTabCenterY(BeatmapGroup g);
/** Left edge x of the group's tab (after init). */
float getTabLeft(BeatmapGroup g);

}  // namespace opsu
