#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "opsu/beatmap/Beatmap.h"

namespace opsu {

/**
 * Data type containing all beatmaps in a beatmap set. Port of itdelatrisu.opsu.beatmap.BeatmapSet.
 *
 * Seams (Options / DB are not ported, so they are explicit parameters, like in Beatmap):
 *  - `ModParams current` = Beatmap.currentModParams() (what `Beatmap.getStarRating()` reads from Options + GameMod).
 *    It is needed by the constructor and by `sortByDifficulty` (the sort key is the star rating with the current mods).
 *  - `useUnicodeMetadata` = Options.useUnicodeMetadata() for `getInfo` / `toString`.
 *  - `BeatmapDB.updateFavoriteStatus(map)` is a hook: `setFavoriteStatusHook` (no hook = nothing is written).
 *  - getInfo reads GameMod::getSpeedMultiplier / getDifficultyMultiplier itself (GameMod is ported).
 *
 * Deviations:
 *  - Java `String.toLowerCase()` in `matches(query)` is ASCII-only here (non-ASCII letters are not folded). The caller
 *    lower-cases the query the same way, so ASCII queries behave identically; "\u00C9" would not match "\u00E9".
 *  - `sortByDifficulty` returns "order changed" by comparing the permutation (Java compared object identity with
 *    `ArrayList.equals`; Beatmap has no `equals`). Same result.
 *  - IndexOutOfBoundsException -> std::out_of_range (`get`, `remove`, `getInfo`, `toString`, `matches` on an empty set).
 *  - The Java Collections.sort is stable (TimSort); std::stable_sort is used. The comparator can in theory be inconsistent
 *    (Beatmap.compareTo wraps ints, see Beatmap.h); Java could then throw "Comparison method violates its general
 *    contract", this does not throw (the order is just unspecified for such absurd input).
 */
class BeatmapSet {
public:
    /** Sorts the beatmaps with sortByDifficulty(current). */
    BeatmapSet(std::vector<Beatmap> beatmaps, const Beatmap::ModParams& current);

    /**
     * osu!lazer's difficulty order: star rating (current mods) ascending, unrated (-1) first, ties by Beatmap.compareTo
     * (OD, then object count), then by version (compareToIgnoreCase). Returns true if the order changed.
     */
    bool sortByDifficulty(const Beatmap::ModParams& current);

    /** The LAZER_ORDER comparator (Java Comparator#compare). */
    static int lazerOrder(const Beatmap& a, const Beatmap& b, const Beatmap::ModParams& current);

    std::size_t size() const { return beatmaps_.size(); }
    /** @throws std::out_of_range */
    const Beatmap& get(std::size_t index) const { return beatmaps_.at(index); }
    Beatmap& get(std::size_t index) { return beatmaps_.at(index); }
    /** Removes and returns the beatmap at the index. @throws std::out_of_range */
    Beatmap remove(std::size_t index);

    std::vector<Beatmap>::const_iterator begin() const { return beatmaps_.begin(); }
    std::vector<Beatmap>::const_iterator end() const { return beatmaps_.end(); }
    std::vector<Beatmap>::iterator begin() { return beatmaps_.begin(); }
    std::vector<Beatmap>::iterator end() { return beatmaps_.end(); }
    const std::vector<Beatmap>& beatmaps() const { return beatmaps_; }

    /**
     * The 5 info lines: [0] "{Artist} - {Title} [{Version}]", [1] "Mapped by {Creator}",
     * [2] "Length: m:ss  BPM: x  Objects: n", [3] "Circles: c  Sliders: s  Spinners: p",
     * [4] "CS:x HP:x AR:x OD:x[ Stars:%.2f]" (values with the HR/EZ multiplier, max 10, DecimalFormat "##.#").
     * @throws std::out_of_range
     */
    std::vector<std::string> getInfo(std::size_t index, bool useUnicodeMetadata, const Beatmap::ModParams& current) const;

    /** "Artist - Title" of the first beatmap. @throws std::out_of_range if the set is empty */
    std::string toString(bool useUnicodeMetadata) const;

    /** Title / artist / creator / source / version / set ID / tags of the first beatmap, version / tags of the rest. `query` is lower-case. */
    bool matches(const std::string& query) const;
    /** type: ar, cs, od, hp, bpm, length, star(s); operator: = == > >= < <=. Any unknown type / operator gives false. */
    bool matches(const std::string& type, const std::string& op, float value) const;

    bool isFavorite() const;
    /** Sets the flag on every beatmap and calls the favourite hook (BeatmapDB.updateFavoriteStatus) for each one. */
    void setFavorite(bool flag);
    bool isPlayed() const;

    /** BeatmapDB.updateFavoriteStatus(map). */
    static void setFavoriteStatusHook(std::function<void(const Beatmap&)> hook);

private:
    std::vector<Beatmap> beatmaps_;
};

}  // namespace opsu
