#pragma once
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapSet.h"

namespace opsu {

/** Measures a string in the font being used (Font.getWidth). */
using TextMeasure = std::function<float(const std::string&)>;

/** BeatmapSetNode.Fit: the last truncated string, so the (slow) text measuring does not run every frame. */
struct TextFit {
    const void* font = nullptr;  // identity of the font (Java compared references)
    std::optional<std::string> src, out;
    float width = 0;
};

/**
 * PARTIAL port (batch 31) of itdelatrisu.opsu.beatmap.BeatmapSetNode: the data and the text half. NOT ported (needs the graphics
 * backend, see the WIP table): `drawSetPanel`, `drawDifficultyRow`, `drawFlash`, the sliced / solid panel images, `drawStarPill`,
 * `fillCircle`, `fillRounded`, `aspect`, `withAlpha`.
 *
 * Seams / deviations
 *  - The node owns the set through a shared_ptr (Java: a reference shared with BeatmapSetList).
 *  - `prev` / `next` are raw, non-owning pointers; BeatmapSetList (not ported) owns the nodes.
 *  - Options is explicit: `getInfo` / `toString` take `useUnicodeMetadata` (+ `ModParams` for the star rating line).
 *  - `truncate` / `fit` take the font as a `TextMeasure` (+ an identity pointer for the cache). `truncate` shortens by whole
 *    CODE POINTS (Java removed UTF-16 units, one at a time, so an astral character took two steps and the middle step measured a
 *    lone surrogate); for BMP text the result is identical. Java `null` strings are not representable: `str` is never null here
 *    (Java `prefix + null` would have printed "null").
 *  - Java quirk kept: the `fit` cache key is font + width + text, NOT the prefix.
 */
class BeatmapSetNode {
public:
    /** The most difficulty icons a set panel shows; larger sets show one icon and a count. */
    static constexpr int MAX_SPREAD_ICONS = 18;
    /** How much bigger the rating pill of a difficulty row is drawn. */
    static constexpr float CHIP_SCALE = 1.25f;
    /** How much taller a difficulty row is than the one the contents were designed for. */
    static constexpr float ROW_HEIGHT_SCALE = 1.5f;
    /** Where the visible bar sits inside the panel image: top / height as a fraction of the image height. */
    static constexpr float PANEL_BAR_TOP = 16.f / 127.f, PANEL_BAR_H = 100.f / 127.f;
    static constexpr float SLICE_LEFT = 0.40f, SLICE_RIGHT = 0.40f;

    explicit BeatmapSetNode(std::shared_ptr<BeatmapSet> set) : beatmapSet_(std::move(set)) {}
    explicit BeatmapSetNode(BeatmapSet set) : beatmapSet_(std::make_shared<BeatmapSet>(std::move(set))) {}

    BeatmapSet& getBeatmapSet() { return *beatmapSet_; }
    const BeatmapSet& getBeatmapSet() const { return *beatmapSet_; }
    const std::shared_ptr<BeatmapSet>& beatmapSetPtr() const { return beatmapSet_; }

    /** The selected beatmap (by `beatmapIndex`), or nullptr if the index is invalid. */
    Beatmap* getSelectedBeatmap();
    const Beatmap* getSelectedBeatmap() const;

    /** The info lines of the selected beatmap, or nullopt (Java null) if none is selected (`beatmapIndex < 0`). @throws std::out_of_range if too large */
    std::optional<std::vector<std::string>> getInfo(bool useUnicodeMetadata, const Beatmap::ModParams& current) const;
    /** "Artist - Title [Version]" of the beatmap at `beatmapIndex`, or "Artist - Title" of the set if it is -1. @throws std::out_of_range */
    std::string toString(bool useUnicodeMetadata) const;

    /** Truncates with "..." so that it fits `maxWidth` (BeatmapSetNode.truncate). */
    static std::string truncate(const TextMeasure& measure, const std::string& str, float maxWidth);
    /** truncate(prefix + str) cached in `c` until the text, font or width changes (BeatmapSetNode.fit). */
    static const std::string& fit(TextFit& c, const void* font, const std::optional<std::string>& prefix, const std::string& str,
                                  float maxWidth, const TextMeasure& measure);

    /** String.format("%.2f", starRating), recomputed only when the rating changes (`ratingValue != starRating`; the cache starts as NaN). */
    const std::string& ratingText(double starRating);

    /** Index of the selected beatmap (-1 if not focused). */
    int beatmapIndex = -1;
    /** Index of this node. */
    int index = 0;
    /** Links to other nodes (non-owning). */
    BeatmapSetNode *prev = nullptr, *next = nullptr;
    /** Smoothed UI state in [0, 1]: hover over the set panel, hover over the difficulty row, "is the selected difficulty". */
    float panelHover = 0, rowHover = 0, select = 0;

    TextFit fitTitle, fitArtist, fitMapper, fitVersion;

private:
    std::shared_ptr<BeatmapSet> beatmapSet_;
    double ratingValue_ = std::numeric_limits<double>::quiet_NaN();
    std::string ratingText_;
};

}  // namespace opsu
