#pragma once
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "opsu/beatmap/Beatmap.h"

namespace opsu {

/**
 * Beatmap sorting orders. Port of itdelatrisu.opsu.beatmap.BeatmapSortOrder.
 *
 * Seams:
 * - BeatmapSetNode/BeatmapSet are not ported yet (UI/state). The comparators work on the set's beatmaps
 *   (`std::vector<Beatmap>`, what `node.getBeatmapSet()` is in Java); BeatmapSetNode will call
 *   `compare(order, a.getBeatmapSet(), b.getBeatmapSet())`.
 * - `Options.getSavedSortOrder()` is injected with setSavedSortOrderProvider() (returns the enum name, e.g. "DATE").
 * - The Java Comparator objects become compare()/less(); results keep the Java contract (sign matters, and the
 *   int-valued orders return `i - j` with wrap-around, not -1/0/1).
 * - Like Java `getBeatmapSet().get(0)`, TITLE/ARTIST/CREATOR/BPM throw std::out_of_range on an empty set.
 */
enum class BeatmapSortOrder : int { TITLE, ARTIST, CREATOR, BPM, LENGTH, DATE, PLAYS };

using BeatmapSetBeatmaps = std::vector<Beatmap>;

constexpr std::size_t BeatmapSortOrder_COUNT = 7;

/** BeatmapSortOrder.values() in declaration order. */
constexpr std::array<BeatmapSortOrder, BeatmapSortOrder_COUNT> beatmapSortOrderValues() {
    return {BeatmapSortOrder::TITLE, BeatmapSortOrder::ARTIST, BeatmapSortOrder::CREATOR, BeatmapSortOrder::BPM,
            BeatmapSortOrder::LENGTH, BeatmapSortOrder::DATE, BeatmapSortOrder::PLAYS};
}

/** Enum constant name, i.e. Java `name()` ("TITLE", ..., "PLAYS"). */
const char* name(BeatmapSortOrder o);
/** Display name, i.e. Java `getName()` ("Title", ..., "Date Added", "Most Played"). */
const char* getName(BeatmapSortOrder o);

/** Java Comparator#compare for this order. */
int compare(BeatmapSortOrder o, const BeatmapSetBeatmaps& v, const BeatmapSetBeatmaps& w);
/** Strict-weak "less" for std::sort / std::stable_sort (Collections.sort is stable: prefer stable_sort). */
inline bool less(BeatmapSortOrder o, const BeatmapSetBeatmaps& v, const BeatmapSetBeatmaps& w) {
    return compare(o, v, w) < 0;
}

/** Hook for Options.getSavedSortOrder(): the saved enum name, or nullopt when none is saved. */
void setSavedSortOrderProvider(std::function<std::optional<std::string>()> provider);
/** Resolves a saved enum name; unknown or absent -> TITLE. */
BeatmapSortOrder savedFromName(const std::optional<std::string>& savedName);
/** BeatmapSortOrder.saved(): uses the provider (TITLE if none is set). */
BeatmapSortOrder saved();
/** BeatmapSortOrder.current(): loads saved() on first use, unless set() was called first. */
BeatmapSortOrder current();
/** BeatmapSortOrder.set(sort). */
void set(BeatmapSortOrder sort);
/** Test helper (no Java counterpart): forgets the current sort so current() reloads. */
void resetForTesting();

}  // namespace opsu
