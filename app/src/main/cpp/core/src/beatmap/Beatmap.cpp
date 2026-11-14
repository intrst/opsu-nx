#include "opsu/beatmap/Beatmap.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include "opsu/GameMod.h"
#include "opsu/logging/Log.h"
#include "opsu/util/JavaMath.h"

namespace opsu {

// ---------------------------------------------------------------------------------------------
// ModParams
// ---------------------------------------------------------------------------------------------
Beatmap::ModParams Beatmap::ModParams::current(float fixedSpeed, float fixedCS) {
    return of(fixedSpeed, GameMod::getSpeedMultiplier(), GameMod::getDifficultyMultiplier(), fixedCS);
}

Beatmap::ModParams Beatmap::ModParams::of(float fixedSpeed, float speedMultiplier, float diffMultiplier,
                                          float fixedCS) {
    ModParams p;
    p.clockRate = ((fixedSpeed > 0.f) ? fixedSpeed : 1.f) * speedMultiplier;
    p.diffMultiplier_ = diffMultiplier;
    p.fixedCS_ = fixedCS;
    // ((long) bits(clockRate) * 31L + bits(diff)) * 31L + bits(fixedCS), with Java's wrap-around
    auto w = [](float f) { return static_cast<uint64_t>(static_cast<int64_t>(java::floatToIntBits(f))); };
    uint64_t k = (w(p.clockRate) * 31u + w(p.diffMultiplier_)) * 31u + w(p.fixedCS_);
    p.key = static_cast<int64_t>(k);
    return p;
}

float Beatmap::ModParams::circleSize(const Beatmap& beatmap) const {
    if (fixedCS_ > 0.f)
        return fixedCS_;
    return java::min(beatmap.circleSize * diffMultiplier_, 10.f);
}

bool Beatmap::ModParams::isNoMod(const Beatmap& beatmap) const {
    return clockRate == 1.f && circleSize(beatmap) == beatmap.circleSize;
}

// ---------------------------------------------------------------------------------------------
// Star rating
// ---------------------------------------------------------------------------------------------
double Beatmap::getStarRating(const ModParams& current) const {
    std::shared_ptr<const ModStars> m = std::atomic_load(&modStars_);
    if (m && m->key == current.key && !current.isNoMod(*this))
        return m->stars;
    return starRating;
}

bool Beatmap::needsModStars(const ModParams& p) const {
    if (p.isNoMod(*this))
        return false;
    std::shared_ptr<const ModStars> m = std::atomic_load(&modStars_);
    return (!m || m->key != p.key);
}

void Beatmap::setModStars(const ModParams& p, double stars, const ModParams& current) {
    if (p.key == current.key)
        std::atomic_store(&modStars_, std::shared_ptr<const ModStars>(new ModStars{p.key, stars}));
}

// ---------------------------------------------------------------------------------------------
// Ordering / text
// ---------------------------------------------------------------------------------------------
int Beatmap::compareTo(const Beatmap& that) const {
    int cmp = java::compare(overallDifficulty, that.overallDifficulty);
    if (cmp == 0) {
        // Java int arithmetic wraps; Integer2.compare(i, j) is `i - j`. Unsigned math = defined wrap in C++.
        uint32_t a = static_cast<uint32_t>(hitObjectCircle) + static_cast<uint32_t>(hitObjectSlider) +
                     static_cast<uint32_t>(hitObjectSpinner);
        uint32_t b = static_cast<uint32_t>(that.hitObjectCircle) + static_cast<uint32_t>(that.hitObjectSlider) +
                     static_cast<uint32_t>(that.hitObjectSpinner);
        cmp = static_cast<int>(a - b);
    }
    return cmp;
}

std::string Beatmap::toString(bool useUnicodeMetadata) const {
    if (!source.empty())
        return source + " (" + getArtist(useUnicodeMetadata) + ") - " + getTitle(useUnicodeMetadata) + " [" +
               version + "]";
    return getArtist(useUnicodeMetadata) + " - " + getTitle(useUnicodeMetadata) + " [" + version + "]";
}

// ---------------------------------------------------------------------------------------------
// (De)serialisation
// ---------------------------------------------------------------------------------------------
std::optional<std::string> Beatmap::breaksToString() const {
    if (!breaks)
        return std::nullopt;
    std::string sb;
    for (int i : *breaks) {
        sb += std::to_string(i);
        sb += ',';
    }
    if (!sb.empty())
        sb.pop_back();
    return sb;
}

void Beatmap::breaksFromString(const std::optional<std::string>& s) {
    if (!s)
        return;
    breaks = std::vector<int>();
    for (const std::string& t : java::split(*s, ','))
        breaks->push_back(java::parseInt(t));
}

std::optional<std::string> Beatmap::timingPointsToString() const {
    if (!timingPoints)
        return std::nullopt;
    std::string sb;
    for (const TimingPoint& p : *timingPoints) {
        sb += p.toString();
        sb += '|';
    }
    if (!sb.empty())
        sb.pop_back();
    return sb;
}

void Beatmap::timingPointsFromString(const std::optional<std::string>& s) {
    timingPoints = std::vector<TimingPoint>();
    if (!s)
        return;
    for (const std::string& t : java::split(*s, '|')) {
        try {
            timingPoints->emplace_back(t);
        } catch (const std::exception& e) {
            Log::warn("Failed to read timing point '" + t + "'.", e);
        }
    }
}

std::optional<std::string> Beatmap::comboToString() const {
    if (!combo)
        return std::nullopt;
    std::string sb;
    for (const Color& c : *combo) {
        sb += std::to_string(c.getRed());
        sb += ',';
        sb += std::to_string(c.getGreen());
        sb += ',';
        sb += std::to_string(c.getBlue());
        sb += '|';
    }
    if (!sb.empty())
        sb.pop_back();
    return sb;
}

void Beatmap::comboFromString(const std::optional<std::string>& s) {
    if (!s)
        return;
    std::vector<Color> colors;
    for (const std::string& t : java::split(*s, '|')) {
        std::vector<std::string> rgb = java::split(t, ',');
        int r = java::parseInt(rgb.at(0));
        int g = java::parseInt(rgb.at(1));
        int b = java::parseInt(rgb.at(2));
        colors.emplace_back(r, g, b);
    }
    if (!colors.empty())
        combo = std::move(colors);
}

std::optional<std::string> Beatmap::sliderBorderToString() const {
    if (!sliderBorder)
        return std::nullopt;
    return std::to_string(sliderBorder->getRed()) + "," + std::to_string(sliderBorder->getGreen()) + "," +
           std::to_string(sliderBorder->getBlue());
}

void Beatmap::sliderBorderFromString(const std::optional<std::string>& s) {
    if (!s)
        return;
    std::vector<std::string> rgb = java::split(*s, ',');
    int r = java::parseInt(rgb.at(0));
    int g = java::parseInt(rgb.at(1));
    int b = java::parseInt(rgb.at(2));
    sliderBorder = Color(r, g, b);
}

// ---------------------------------------------------------------------------------------------
// Play stats
// ---------------------------------------------------------------------------------------------
void Beatmap::incrementPlayCounter() {
    using namespace std::chrono;
    incrementPlayCounter(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

void Beatmap::incrementPlayCounter(int64_t nowMillis) {
    playCount++;
    lastPlayed = nowMillis;
}

void Beatmap::copyAdditionalFields(Beatmap& target) const {
    target.starRating = starRating;
    target.dateAdded = dateAdded;
    target.favorite = favorite;
    target.playCount = playCount;
    target.lastPlayed = lastPlayed;
    target.localMusicOffset = localMusicOffset;
}

}  // namespace opsu
