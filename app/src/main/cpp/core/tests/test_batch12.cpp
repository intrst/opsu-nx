// Tests for DownloadImporter. Behavioural, no JVM golden.
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include "opsu/beatmap/DownloadImporter.h"
#include "opsu/logging/StubLogger.h"
#include "opsu/logging/Log.h"

using namespace opsu;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

struct Env : DownloadImporterEnv {
    std::string dl, skinRoot = "/skins", bmDir = "/songs";
    bool throwSkins = false, throwBeatmaps = false, throwOdd = false;
    std::atomic<int> skinCalls{0}, beatmapCalls{0};
    std::string skinSrc, skinDest, bmSrc, bmDest;
    std::thread::id skinThread, bmThread, caller = std::this_thread::get_id();
    std::atomic<bool> skinDone{false};
    int sleepMs = 0;
    bool skinDoneWhenBeatmapsReturned = false;
    std::string getDownloadDir() override { return dl; }
    std::string getSkinRootDir() override { return skinRoot; }
    std::string getBeatmapDir() override { return bmDir; }
    void unpackSkins(const std::string& s, const std::string& d) override {
        skinCalls++; skinSrc = s; skinDest = d; skinThread = std::this_thread::get_id();
        if (sleepMs) std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        if (throwSkins) throw std::runtime_error("skin boom");
        skinDone = true;
    }
    void unpackBeatmaps(const std::string& s, const std::string& d) override {
        beatmapCalls++; bmSrc = s; bmDest = d; bmThread = std::this_thread::get_id();
        if (throwOdd) throw 42;
        if (throwBeatmaps) throw std::runtime_error("osz boom");
    }
};

int main() {
    Log::setLogger(&StubLogger::get());

    fs::path tmp = fs::temp_directory_path() / "opsu_batch12_dl";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    fs::path file = tmp / "not_a_dir.txt";
    { FILE* f = std::fopen(file.string().c_str(), "w"); if (f) std::fclose(f); }

    // null / missing / not-a-directory download dir: nothing runs
    {
        Env e; e.dl = "";
        DownloadImporter::importAll(e);
        CHECK(e.skinCalls == 0 && e.beatmapCalls == 0);
        e.dl = (tmp / "missing").string();
        DownloadImporter::importAll(e);
        CHECK(e.skinCalls == 0 && e.beatmapCalls == 0);
        e.dl = file.string();
        DownloadImporter::importAll(e);
        CHECK(e.skinCalls == 0 && e.beatmapCalls == 0);
    }
    // normal run: both unpackers get (downloadDir, their destination); skins on another thread; beatmaps on the caller
    {
        Env e; e.dl = tmp.string(); e.sleepMs = 50;
        DownloadImporter::importAll(e);
        CHECK(e.skinCalls == 1 && e.beatmapCalls == 1);
        CHECK(e.skinSrc == e.dl && e.skinDest == "/skins");
        CHECK(e.bmSrc == e.dl && e.bmDest == "/songs");
        CHECK(e.skinThread != e.caller && e.bmThread == e.caller);
        CHECK(e.skinDone);   // importAll joins: the slow skin thread has finished when it returns
    }
    // a throwing beatmap unpacker does not stop the skin import or escape
    {
        Env e; e.dl = tmp.string(); e.throwBeatmaps = true; e.sleepMs = 20;
        DownloadImporter::importAll(e);
        CHECK(e.beatmapCalls == 1 && e.skinCalls == 1 && e.skinDone);
    }
    // non-std exception is swallowed too (Java catches Exception)
    {
        Env e; e.dl = tmp.string(); e.throwOdd = true;
        DownloadImporter::importAll(e);
        CHECK(e.skinDone);
    }
    // a throwing skin unpacker does not escape the thread (would std::terminate) and beatmaps still run
    {
        Env e; e.dl = tmp.string(); e.throwSkins = true;
        DownloadImporter::importAll(e);
        CHECK(e.skinCalls == 1 && e.beatmapCalls == 1 && !e.skinDone);
    }
    fs::remove_all(tmp);
    Log::setLogger(nullptr);

    if (failures) { std::printf("batch12: %d FAILED\n", failures); return 1; }
    std::printf("batch12: all tests passed\n");
    return 0;
}
