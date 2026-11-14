#pragma once
#include <string>

namespace opsu {

/**
 * Seam for what DownloadImporter reads from Options and calls on the unpackers
 * (Options.getDownloadDir / getSkinRootDir / getBeatmapDir, SkinUnpacker / OszUnpacker.unpackAllFiles).
 * Paths are UTF-8 strings; "" == Java null. Implementations may throw: the importer catches everything.
 */
class DownloadImporterEnv {
public:
    virtual ~DownloadImporterEnv() = default;
    virtual std::string getDownloadDir() = 0;
    virtual std::string getSkinRootDir() = 0;
    virtual std::string getBeatmapDir() = 0;
    /** SkinUnpacker.unpackAllFiles(src, dest). Runs on a background thread. */
    virtual void unpackSkins(const std::string& src, const std::string& dest) = 0;
    /** OszUnpacker.unpackAllFiles(src, dest). Runs on the calling thread. */
    virtual void unpackBeatmaps(const std::string& src, const std::string& dest) = 0;
};

/** Imports beatmaps (.osz) and skins (.osk) found in the Download folder. Port of itdelatrisu.opsu.beatmap.DownloadImporter. */
namespace DownloadImporter {
/**
 * Imports every .osz and .osk in the Download folder (skins are unpacked alongside the beatmaps).
 * Returns at once if the folder is unset or not a directory. Errors are logged, never thrown.
 * Blocks until both unpackers have finished.
 */
void importAll(DownloadImporterEnv& env);
}  // namespace DownloadImporter

}  // namespace opsu
