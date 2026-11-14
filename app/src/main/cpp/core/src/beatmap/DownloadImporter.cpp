#include "opsu/beatmap/DownloadImporter.h"
#include <exception>
#include <filesystem>
#include <system_error>
#include <thread>
#include "opsu/logging/Log.h"

namespace opsu {
namespace DownloadImporter {

void importAll(DownloadImporterEnv& env) {
    const std::string dir = env.getDownloadDir();
    std::error_code ec;
    if (dir.empty() || !std::filesystem::is_directory(std::filesystem::u8path(dir), ec))
        return;

    // skins on their own thread (Java: daemon thread "opsu-download-skins"; joined below, so daemon-ness is moot)
    std::thread skinThread([&env, dir]() {
        try {
            env.unpackSkins(dir, env.getSkinRootDir());
        } catch (const std::exception& e) {
            Log::error(e);
        } catch (...) {
            Log::error("DownloadImporter: skin import failed");
        }
    });

    try {
        env.unpackBeatmaps(dir, env.getBeatmapDir());
    } catch (const std::exception& e) {
        Log::error(e);  // never block startup because of the Download folder
    } catch (...) {
        Log::error("DownloadImporter: beatmap import failed");
    }

    skinThread.join();
}

}  // namespace DownloadImporter
}  // namespace opsu
