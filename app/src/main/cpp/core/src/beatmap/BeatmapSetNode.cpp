#include "opsu/beatmap/BeatmapSetNode.h"
#include <stdexcept>
#include "opsu/util/JavaCompat.h"

namespace opsu {

Beatmap* BeatmapSetNode::getSelectedBeatmap() {
    return (beatmapIndex < 0 || static_cast<std::size_t>(beatmapIndex) >= beatmapSet_->size()) ? nullptr
                                                                                                : &beatmapSet_->get(beatmapIndex);
}
const Beatmap* BeatmapSetNode::getSelectedBeatmap() const {
    return (beatmapIndex < 0 || static_cast<std::size_t>(beatmapIndex) >= beatmapSet_->size()) ? nullptr
                                                                                                : &beatmapSet_->get(beatmapIndex);
}

std::optional<std::vector<std::string>> BeatmapSetNode::getInfo(bool useUnicodeMetadata, const Beatmap::ModParams& current) const {
    if (beatmapIndex < 0) return std::nullopt;
    return beatmapSet_->getInfo(static_cast<std::size_t>(beatmapIndex), useUnicodeMetadata, current);
}

std::string BeatmapSetNode::toString(bool useUnicodeMetadata) const {
    if (beatmapIndex == -1) return beatmapSet_->toString(useUnicodeMetadata);
    if (beatmapIndex < 0) throw std::out_of_range("BeatmapSetNode::toString");  // Java: ArrayList.get(-2)
    return beatmapSet_->get(static_cast<std::size_t>(beatmapIndex)).toString(useUnicodeMetadata);
}

std::string BeatmapSetNode::truncate(const TextMeasure& measure, const std::string& str, float maxWidth) {
    if (measure(str) <= maxWidth) return str;
    // byte offset of the end of the first n code points: ends[n]
    std::vector<std::size_t> ends{0};
    for (std::size_t i = 0; i < str.size();) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        std::size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        std::size_t j = i + 1;
        while (j < str.size() && j < i + n && (static_cast<unsigned char>(str[j]) & 0xC0) == 0x80) j++;  // never run past a broken sequence
        i = j;
        ends.push_back(i);
    }
    std::size_t len = ends.size() - 1;
    while (len > 1 && measure(str.substr(0, ends[len]) + "...") > maxWidth) len--;
    return str.substr(0, ends[len]) + "...";
}

const std::string& BeatmapSetNode::fit(TextFit& c, const void* font, const std::optional<std::string>& prefix, const std::string& str,
                                       float maxWidth, const TextMeasure& measure) {
    if (c.out && c.font == font && c.width == maxWidth && c.src && *c.src == str) return *c.out;
    c.font = font;
    c.width = maxWidth;
    c.src = str;
    c.out = truncate(measure, prefix ? *prefix + str : str, maxWidth);
    return *c.out;
}

const std::string& BeatmapSetNode::ratingText(double starRating) {
    if (ratingValue_ != starRating) {  // formatted once per rating, not every frame (NaN never equals: always re-formatted)
        ratingValue_ = starRating;
        ratingText_ = java::formatFixed(starRating, 2);
    }
    return ratingText_;
}

}  // namespace opsu
