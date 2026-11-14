#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "opsu/beatmap/HitObject.h"
#include "opsu/beatmap/TimingPoint.h"
#include "opsu/util/Color.h"

namespace opsu {

/**
 * Data type representing a beatmap (one .osu file). Port of itdelatrisu.opsu.beatmap.Beatmap.
 *
 * Fields stay public and keep their Java names so BeatmapParser / BeatmapDB port mechanically.
 *
 * Java -> C++ mapping:
 *  - File            -> std::string path ("" = Java null)
 *  - String (null)   -> std::string ("" = null for md5Hash and the file paths)
 *  - nullable arrays/lists/colours -> std::optional (null-ness matters: "timingPoints == null" means
 *                                     "not loaded from the DB yet", an empty list does not)
 *  - Options.*       -> explicit parameters (useUnicodeMetadata, skin fallbacks, fixed speed / CS)
 *
 * NOT ported (needs the graphics backend): the background-image LRU cache and
 * loadBackground / isBackgroundLoading / hasLoadedBackground / drawBackground / clearBackgroundImageCache.
 */
class Beatmap {
public:
    static constexpr int8_t MODE_OSU = 0, MODE_TAIKO = 1, MODE_CTB = 2, MODE_MANIA = 3;

    /**
     * Mod-dependent parameters that affect the star rating: clock rate (speed mods + fixed speed),
     * difficulty multiplier (HR/EZ) and fixed CS. Port of Beatmap.ModParams (an immutable snapshot).
     */
    struct ModParams {
        float clockRate = 1.f;
        /** Hash of the three values above; two snapshots with the same key give the same stars. */
        int64_t key = 0;

        /** Snapshot of the current mods. fixedSpeed/fixedCS are Options.getFixedSpeed()/getFixedCS(). */
        static ModParams current(float fixedSpeed, float fixedCS);
        /** Snapshot from explicit values (what `current` does after reading GameMod). */
        static ModParams of(float fixedSpeed, float speedMultiplier, float diffMultiplier, float fixedCS);

        /** The circle size with mods applied (fixed CS wins, else CS * difficulty multiplier, max 10). */
        float circleSize(const Beatmap& beatmap) const;
        /** True if these parameters change neither the speed nor the circle size of the beatmap. */
        bool isNoMod(const Beatmap& beatmap) const;

    private:
        float diffMultiplier_ = 1.f, fixedCS_ = 0.f;
    };

    explicit Beatmap(std::string osuFile) : file_(std::move(osuFile)) {}

    const std::string& getFile() const { return file_; }

    // ---- star rating -------------------------------------------------------------------------
    /** Star rating for the current mods if cached (and mods matter), else the plain rating. */
    double getStarRating(const ModParams& current) const;
    /** Whether stars for these (non-trivial) mods still have to be computed. */
    bool needsModStars(const ModParams& p) const;
    /** Stores mod stars, unless `p` is no longer the current mods (stale result is dropped). */
    void setModStars(const ModParams& p, double stars, const ModParams& current);

    // ---- accessors with Options seams --------------------------------------------------------
    /** Options.useUnicodeMetadata() is `useUnicodeMetadata`. */
    const std::string& getTitle(bool useUnicodeMetadata) const {
        return (useUnicodeMetadata && !titleUnicode.empty()) ? titleUnicode : title;
    }
    const std::string& getArtist(bool useUnicodeMetadata) const {
        return (useUnicodeMetadata && !artistUnicode.empty()) ? artistUnicode : artist;
    }
    /** Own combo colours, or `skinComboColors` (Options.getSkin().getComboColors()). */
    const std::vector<Color>& getComboColors(const std::vector<Color>& skinComboColors) const {
        return combo ? *combo : skinComboColors;
    }
    const Color& getSliderBorderColor(const Color& skinSliderBorder) const {
        return sliderBorder ? *sliderBorder : skinSliderBorder;
    }

    // ---- ordering / text ---------------------------------------------------------------------
    /** Compares by overall difficulty, then by object count (Java: Comparable<Beatmap>). */
    int compareTo(const Beatmap& that) const;
    bool operator<(const Beatmap& that) const { return compareTo(that) < 0; }
    /** "[source (]artist[)] - title [version]". */
    std::string toString(bool useUnicodeMetadata) const;

    // ---- database (de)serialisation ----------------------------------------------------------
    /** Comma-separated break times, or nullopt if `breaks` is null. */
    std::optional<std::string> breaksToString() const;
    /** @throws java::NumberFormatException on bad tokens (like Java). A null string leaves breaks alone. */
    void breaksFromString(const std::optional<std::string>& s);
    std::optional<std::string> timingPointsToString() const;
    /** Always leaves a non-null (possibly empty) list; unparsable points are skipped with a Log::warn. */
    void timingPointsFromString(const std::optional<std::string>& s);
    std::optional<std::string> comboToString() const;
    /** @throws java::NumberFormatException / std::out_of_range on malformed input (like Java). */
    void comboFromString(const std::optional<std::string>& s);
    std::optional<std::string> sliderBorderToString() const;
    void sliderBorderFromString(const std::optional<std::string>& s);

    // ---- play statistics ---------------------------------------------------------------------
    /** playCount++ and lastPlayed = now (ms since epoch). */
    void incrementPlayCounter();
    void incrementPlayCounter(int64_t nowMillis);
    /** Copies the DB-owned fields (star rating, date added, favourite, play stats, music offset). */
    void copyAdditionalFields(Beatmap& target) const;

    // ---- fields (see the .osu file format) ---------------------------------------------------
    // "General"
    std::string md5Hash;               // "" = not set
    double starRating = -1;
    int64_t dateAdded = 0;
    bool favorite = false;
    int playCount = 0;
    int64_t lastPlayed = 0;
    int localMusicOffset = 0;

    std::string audioFilename;         // "" = null
    int audioLeadIn = 0;
    int previewTime = -1;
    int8_t countdown = 0;
    std::string sampleSet;
    float stackLeniency = 0.7f;
    int8_t mode = MODE_OSU;
    bool letterboxInBreaks = false;
    bool widescreenStoryboard = false;
    bool epilepsyWarning = false;

    // "Metadata"
    std::string title, titleUnicode;
    std::string artist, artistUnicode;
    std::string creator;
    std::string version;
    std::string source;
    std::string tags;
    int beatmapID = 0;
    int beatmapSetID = 0;

    // "Difficulty"
    float HPDrainRate = 5.f;
    float circleSize = 5.f;
    float overallDifficulty = 5.f;
    float approachRate = -1.f;
    float sliderMultiplier = 1.4f;
    float sliderTickRate = 1.f;

    // "Events"
    std::string bg;                    // "" = null
    std::string video;                 // "" = null
    int videoOffset = 0;
    std::optional<std::vector<int>> breaks;  // start/end pairs

    // "TimingPoints"
    std::optional<std::vector<TimingPoint>> timingPoints;
    int bpmMin = 0, bpmMax = 0;

    // "Colours"
    std::optional<std::vector<Color>> combo;
    std::optional<Color> sliderBorder;

    // "HitObjects"
    std::optional<std::vector<HitObject>> objects;
    int hitObjectCircle = 0, hitObjectSlider = 0, hitObjectSpinner = 0;
    int endTime = -1;

private:
    struct ModStars {
        int64_t key;
        double stars;
    };

    std::string file_;
    /** Immutable snapshot, swapped atomically (Java: a volatile reference). */
    std::shared_ptr<const ModStars> modStars_;
};

}  // namespace opsu
