#include "opsu/db/BeatmapDB.h"
#include <cstdint>
#include "opsu/downloads/Download.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

namespace {
// java.io.File: name of the parent directory / name of the file (trailing and duplicate '/' ignored).
void splitFile(const std::string& path, std::string& dir, std::string& file) {
    std::string p;
    for (char ch : path) if (!(ch == '/' && !p.empty() && p.back() == '/')) p += ch;
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    size_t i = p.rfind('/');
    // Java: getParentFile() is null for a bare file name -> NullPointerException (not an SQLException: propagates).
    if (i == std::string::npos) throw std::logic_error("Beatmap file has no parent directory: '" + path + "'");
    file = p.substr(i + 1);
    std::string parent = (i == 0) ? "/" : p.substr(0, i);
    size_t j = parent.rfind('/');
    dir = (parent == "/") ? "" : parent.substr(j + 1);
}
int wrapSub(int a, int b) { return static_cast<int>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
}

void BeatmapDB::updateCacheSize() {
    if (store_ == nullptr) return;
    try {
        store_->updateCacheSize(std::to_string(cacheSize_ > 0 ? cacheSize_ : 0));
    } catch (const SQLException& e) {
        Download::reportError("Could not update beatmap cache size.", &e, true);
    }
}

void BeatmapDB::deleteMap(const std::string& dir, const std::string& file) {
    if (store_ == nullptr) return;
    try {
        cacheSize_ = wrapSub(cacheSize_, store_->deleteMap(dir, file));
        updateCacheSize();
    } catch (const SQLException& e) {
        Download::reportError("Failed to delete beatmap entry from database.", &e, true);
    }
}

void BeatmapDB::deleteGroup(const std::string& dir) {
    if (store_ == nullptr) return;
    try {
        cacheSize_ = wrapSub(cacheSize_, store_->deleteGroup(dir));
        updateCacheSize();
    } catch (const SQLException& e) {
        Download::reportError("Failed to delete beatmap group entry from database.", &e, true);
    }
}

void BeatmapDB::setStars(const Beatmap& b, bool u) {
    if (store_ == nullptr) return;
    try {
        std::string d, f; splitFile(b.getFile(), d, f);
        store_->setStars(b.starRating, d, f);
    } catch (const SQLException& e) {
        Download::reportError("Failed to save star rating '" + java::formatFixed(b.starRating, 4) + "' for beatmap '" + b.toString(u) +
                              "' in database.", &e, true);
    }
}

void BeatmapDB::updatePlayStatistics(const Beatmap& b, bool u) {
    if (store_ == nullptr) return;
    try {
        std::string d, f; splitFile(b.getFile(), d, f);
        store_->updatePlayStats(b.playCount, b.lastPlayed, d, f);
    } catch (const SQLException& e) {
        Download::reportError("Failed to update play statistics for beatmap '" + b.toString(u) + "' in database.", &e, true);
    }
}

void BeatmapDB::updateFavoriteStatus(const Beatmap& b, bool u) {
    if (store_ == nullptr) return;
    try {
        std::string d, f; splitFile(b.getFile(), d, f);
        store_->setFavorite(b.favorite, d, f);
    } catch (const SQLException& e) {
        Download::reportError("Failed to update favorite status for beatmap '" + b.toString(u) + "' in database.", &e, true);
    }
}

void BeatmapDB::updateLocalOffset(const Beatmap& b, bool u) {
    if (store_ == nullptr) return;
    try {
        std::string d, f; splitFile(b.getFile(), d, f);
        store_->setLocalOffset(b.localMusicOffset, d, f);
    } catch (const SQLException& e) {
        Download::reportError("Failed to update local music offset for beatmap '" + b.toString(u) + "' in database.", &e, true);
    }
}

}  // namespace opsu
