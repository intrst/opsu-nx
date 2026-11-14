#include "opsu/beatmap/BeatmapSortOrder.h"
#include <cstring>
#include <utility>
#include "opsu/util/JavaIO.h"
#include "opsu/util/JavaMath.h"

namespace opsu {

namespace {
std::function<std::optional<std::string>()> savedProvider;
BeatmapSortOrder currentSort = BeatmapSortOrder::TITLE;
bool loaded = false;
}  // namespace

const char* name(BeatmapSortOrder o) {
    switch (o) {
        case BeatmapSortOrder::TITLE:   return "TITLE";
        case BeatmapSortOrder::ARTIST:  return "ARTIST";
        case BeatmapSortOrder::CREATOR: return "CREATOR";
        case BeatmapSortOrder::BPM:     return "BPM";
        case BeatmapSortOrder::LENGTH:  return "LENGTH";
        case BeatmapSortOrder::DATE:    return "DATE";
        case BeatmapSortOrder::PLAYS:   return "PLAYS";
    }
    return "";
}

const char* getName(BeatmapSortOrder o) {
    switch (o) {
        case BeatmapSortOrder::TITLE:   return "Title";
        case BeatmapSortOrder::ARTIST:  return "Artist";
        case BeatmapSortOrder::CREATOR: return "Creator";
        case BeatmapSortOrder::BPM:     return "BPM";
        case BeatmapSortOrder::LENGTH:  return "Length";
        case BeatmapSortOrder::DATE:    return "Date Added";
        case BeatmapSortOrder::PLAYS:   return "Most Played";
    }
    return "";
}

int compare(BeatmapSortOrder o, const BeatmapSetBeatmaps& v, const BeatmapSetBeatmaps& w) {
    switch (o) {
        case BeatmapSortOrder::TITLE:
            return java::compareToIgnoreCase(v.at(0).title, w.at(0).title);
        case BeatmapSortOrder::ARTIST:
            return java::compareToIgnoreCase(v.at(0).artist, w.at(0).artist);
        case BeatmapSortOrder::CREATOR:
            return java::compareToIgnoreCase(v.at(0).creator, w.at(0).creator);
        case BeatmapSortOrder::BPM:
            return java::integer2Compare(v.at(0).bpmMax, w.at(0).bpmMax);
        case BeatmapSortOrder::LENGTH: {  // longest beatmap in each set (starts at 0, like Java)
            int vMax = 0, wMax = 0;
            for (const Beatmap& b : v) if (b.endTime > vMax) vMax = b.endTime;
            for (const Beatmap& b : w) if (b.endTime > wMax) wMax = b.endTime;
            return java::integer2Compare(vMax, wMax);
        }
        case BeatmapSortOrder::DATE: {  // latest beatmap added in each set
            int64_t vMax = 0, wMax = 0;
            for (const Beatmap& b : v) if (b.dateAdded > vMax) vMax = b.dateAdded;
            for (const Beatmap& b : w) if (b.dateAdded > wMax) wMax = b.dateAdded;
            return java::long2Compare(vMax, wMax);
        }
        case BeatmapSortOrder::PLAYS: {  // total plays per set (int sum wraps like Java)
            uint32_t vTotal = 0, wTotal = 0;
            for (const Beatmap& b : v) vTotal += static_cast<uint32_t>(b.playCount);
            for (const Beatmap& b : w) wTotal += static_cast<uint32_t>(b.playCount);
            return java::integer2Compare(static_cast<int32_t>(vTotal), static_cast<int32_t>(wTotal));
        }
    }
    return 0;
}

void setSavedSortOrderProvider(std::function<std::optional<std::string>()> provider) {
    savedProvider = std::move(provider);
}

BeatmapSortOrder savedFromName(const std::optional<std::string>& savedName) {
    if (savedName) {
        for (BeatmapSortOrder s : beatmapSortOrderValues())
            if (*savedName == name(s)) return s;
    }
    return BeatmapSortOrder::TITLE;
}

BeatmapSortOrder saved() {
    return savedFromName(savedProvider ? savedProvider() : std::nullopt);
}

BeatmapSortOrder current() {
    if (!loaded) {
        loaded = true;
        currentSort = saved();
    }
    return currentSort;
}

void set(BeatmapSortOrder sort) {
    loaded = true;
    currentSort = sort;
}

void resetForTesting() {
    loaded = false;
    currentSort = BeatmapSortOrder::TITLE;
    savedProvider = nullptr;
}

}  // namespace opsu
