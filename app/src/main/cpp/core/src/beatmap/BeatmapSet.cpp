#include "opsu/beatmap/BeatmapSet.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <numeric>
#include <stdexcept>
#include "opsu/GameMod.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"
#include "opsu/util/JavaMath.h"

namespace opsu {

namespace {
std::function<void(const Beatmap&)> favoriteHook;

/** Java (long) cast of a float: NaN -> 0, saturating. */
int64_t toLong(float v) {
    if (std::isnan(v)) return 0;
    if (v >= 9223372036854775807.0f) return std::numeric_limits<int64_t>::max();
    if (v <= -9223372036854775808.0f) return std::numeric_limits<int64_t>::min();
    return static_cast<int64_t>(v);
}

/** String.toLowerCase() for ASCII (see the header). */
std::string lower(const std::string& s) {
    std::string r = s;
    for (char& c : r)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return r;
}

bool contains(const std::string& s, const std::string& q) { return s.find(q) != std::string::npos; }

std::string fmt(const char* f, long long a, long long b) {
    char buf[64];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}
}  // namespace

BeatmapSet::BeatmapSet(std::vector<Beatmap> beatmaps, const Beatmap::ModParams& current)
    : beatmaps_(std::move(beatmaps)) {
    sortByDifficulty(current);
}

int BeatmapSet::lazerOrder(const Beatmap& a, const Beatmap& b, const Beatmap::ModParams& current) {
    int cmp = java::compare(a.getStarRating(current), b.getStarRating(current));
    if (cmp == 0) cmp = a.compareTo(b);  // OD, then object count
    if (cmp == 0) cmp = java::compareToIgnoreCase(a.version, b.version);
    return cmp;
}

bool BeatmapSet::sortByDifficulty(const Beatmap::ModParams& current) {
    std::vector<std::size_t> order(beatmaps_.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
        return lazerOrder(beatmaps_[x], beatmaps_[y], current) < 0;
    });
    bool changed = false;
    for (std::size_t i = 0; i < order.size(); i++)
        if (order[i] != i) { changed = true; break; }
    if (changed) {
        std::vector<Beatmap> sorted;
        sorted.reserve(beatmaps_.size());
        for (std::size_t i : order) sorted.push_back(std::move(beatmaps_[i]));
        beatmaps_ = std::move(sorted);
    }
    return changed;
}

Beatmap BeatmapSet::remove(std::size_t index) {
    if (index >= beatmaps_.size()) throw std::out_of_range("BeatmapSet::remove");
    Beatmap removed = std::move(beatmaps_[index]);
    beatmaps_.erase(beatmaps_.begin() + static_cast<std::ptrdiff_t>(index));
    return removed;
}

std::vector<std::string> BeatmapSet::getInfo(std::size_t index, bool useUnicodeMetadata,
                                             const Beatmap::ModParams& current) const {
    const Beatmap& beatmap = beatmaps_.at(index);
    float speedModifier = GameMod::getSpeedMultiplier();
    int64_t endTime = toLong(static_cast<float>(beatmap.endTime) / speedModifier);
    int bpmMin = java::toInt(beatmap.bpmMin * speedModifier);
    int bpmMax = java::toInt(beatmap.bpmMax * speedModifier);
    float multiplier = GameMod::getDifficultyMultiplier();
    auto nf = [](float v) { return java::formatTrimmed(static_cast<double>(v), 1); };  // new DecimalFormat("##.#")

    std::vector<std::string> info(5);
    info[0] = beatmap.toString(useUnicodeMetadata);
    info[1] = "Mapped by " + beatmap.creator;

    int64_t minutes = endTime / 60000;               // TimeUnit.MILLISECONDS.toMinutes (truncating)
    int64_t seconds = endTime / 1000 - minutes * 60;  // toSeconds(ms) - MINUTES.toSeconds(minutes)
    std::string bpm = (bpmMax <= 0) ? "--" : (bpmMin == bpmMax ? std::to_string(bpmMin)
                                                               : fmt("%lld-%lld", bpmMin, bpmMax));
    // Java int addition wraps
    int objects = static_cast<int>(static_cast<uint32_t>(beatmap.hitObjectCircle) +
                                   static_cast<uint32_t>(beatmap.hitObjectSlider) +
                                   static_cast<uint32_t>(beatmap.hitObjectSpinner));
    info[2] = "Length: " + fmt("%lld:%02lld", minutes, seconds) + "  BPM: " + bpm + "  Objects: " + std::to_string(objects);
    info[3] = "Circles: " + std::to_string(beatmap.hitObjectCircle) + "  Sliders: " +
              std::to_string(beatmap.hitObjectSlider) + "  Spinners: " + std::to_string(beatmap.hitObjectSpinner);
    double stars = beatmap.getStarRating(current);
    info[4] = "CS:" + nf(java::min(beatmap.circleSize * multiplier, 10.f)) +
              " HP:" + nf(java::min(beatmap.HPDrainRate * multiplier, 10.f)) +
              " AR:" + nf(java::min(beatmap.approachRate * multiplier, 10.f)) +
              " OD:" + nf(java::min(beatmap.overallDifficulty * multiplier, 10.f)) +
              (stars >= 0 ? " Stars:" + java::formatFixed(stars, 2) : "");
    return info;
}

std::string BeatmapSet::toString(bool useUnicodeMetadata) const {
    const Beatmap& beatmap = beatmaps_.at(0);
    return beatmap.getArtist(useUnicodeMetadata) + " - " + beatmap.getTitle(useUnicodeMetadata);
}

bool BeatmapSet::matches(const std::string& query) const {
    const Beatmap& first = beatmaps_.at(0);
    if (contains(lower(first.title), query) || contains(lower(first.titleUnicode), query) ||
        contains(lower(first.artist), query) || contains(lower(first.artistUnicode), query) ||
        contains(lower(first.creator), query) || contains(lower(first.source), query) ||
        contains(lower(first.version), query) || contains(std::to_string(first.beatmapSetID), query) ||
        contains(first.tags, query))
        return true;
    for (std::size_t i = 1; i < beatmaps_.size(); i++) {
        const Beatmap& b = beatmaps_[i];
        if (contains(lower(b.version), query) || contains(b.tags, query)) return true;
    }
    return false;
}

bool BeatmapSet::matches(const std::string& type, const std::string& op, float value) const {
    for (const Beatmap& beatmap : beatmaps_) {
        float v;
        if (type == "ar") v = beatmap.approachRate;
        else if (type == "cs") v = beatmap.circleSize;
        else if (type == "od") v = beatmap.overallDifficulty;
        else if (type == "hp") v = beatmap.HPDrainRate;
        else if (type == "bpm") v = static_cast<float>(beatmap.bpmMax);
        else if (type == "length") v = static_cast<float>(beatmap.endTime / 1000);  // int division first
        else if (type == "star" || type == "stars")
            v = static_cast<float>(java::round(beatmap.starRating * 100)) / 100.f;
        else return false;

        bool met;
        if (op == "=" || op == "==") met = (v == value);
        else if (op == ">") met = (v > value);
        else if (op == ">=") met = (v >= value);
        else if (op == "<") met = (v < value);
        else if (op == "<=") met = (v <= value);
        else return false;

        if (met) return true;
    }
    return false;
}

bool BeatmapSet::isFavorite() const {
    for (const Beatmap& m : beatmaps_)
        if (m.favorite) return true;
    return false;
}

void BeatmapSet::setFavorite(bool flag) {
    for (Beatmap& m : beatmaps_) {
        m.favorite = flag;
        if (favoriteHook) favoriteHook(m);
    }
}

bool BeatmapSet::isPlayed() const {
    for (const Beatmap& m : beatmaps_)
        if (m.playCount > 0) return true;
    return false;
}

void BeatmapSet::setFavoriteStatusHook(std::function<void(const Beatmap&)> hook) { favoriteHook = std::move(hook); }

}  // namespace opsu
