// T2: BASS audio backend. Port of BassAudio.java + BassBackend.java.
#include "BassBackend.h"

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <mutex>
#include <unordered_map>

#include "../../third_party/bass/bass.h"
#include "../../third_party/bass/bass_fx.h"

#ifdef __ANDROID__
#include <android/asset_manager.h>
#endif

#include "BassClip.h"
#include "BassMusic.h"
#include "BassStreamClip.h"
#include "opsu/logging/Log.h"
#include "opsu/util/JavaIO.h"

namespace opsu {
namespace bass {

namespace {

const char* const TAG = "BassAudio";

/** Output sample rate. 44100 matches most beatmap audio; BASS resamples anything else. */
constexpr DWORD SAMPLE_RATE = 44100;
/** How often (ms) the device latency is re-read; it changes with the output route (speaker/wired/Bluetooth). */
constexpr int64_t LATENCY_POLL_MS = 1000;

std::mutex g_initMutex;
bool g_initialized = false;  // guarded by g_initMutex

std::atomic<bool> g_pollLatency{false};
std::atomic<float> g_outputLatencyMs{0.f};
std::atomic<int64_t> g_lastLatencyPoll{INT64_MIN};

// event queue (BASS thread -> game thread)
std::mutex g_eventMutex;
std::vector<BassAudio::Event> g_events;
BassAudio::EventPoster g_poster;

// sync sinks, looked up by id from the BASS thread (the callback only gets one void*)
std::mutex g_sinkMutex;
std::unordered_map<uintptr_t, std::shared_ptr<SyncSink>> g_sinks;
std::atomic<uintptr_t> g_nextSinkId{1};

std::atomic<AAssetManager*> g_assets{nullptr};
std::atomic<void*> g_javaVM{nullptr};

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/** Runs on BASS's thread. Never runs game logic: marks the sink and queues an event. */
void CALLBACK endSyncProc(HSYNC /*handle*/, DWORD /*channel*/, DWORD /*data*/, void* user) {
    std::shared_ptr<SyncSink> sink;
    {
        std::lock_guard<std::mutex> lk(g_sinkMutex);
        auto it = g_sinks.find(reinterpret_cast<uintptr_t>(user));
        if (it == g_sinks.end()) return;  // already released
        sink = it->second;
    }
    sink->fired.store(true, std::memory_order_release);
    BassAudio::postEvent([sink]() {
        std::function<void()> fn = sink->onGameThread;  // copy: the handler may dispose its owner
        if (fn) fn();
    });
}

}  // namespace

// ---------------------------------------------------------------------------------------------------
// lifecycle (BassAudio.java)
// ---------------------------------------------------------------------------------------------------

void BassAudio::setJavaVM(void* javaVM) { g_javaVM.store(javaVM); }

bool BassAudio::init() {
    std::lock_guard<std::mutex> lk(g_initMutex);
    if (g_initialized) return true;

    // The headers and the .so files must be the same BASS API generation.
    const DWORD ver = BASS_GetVersion();
    if (HIWORD(ver) != BASSVERSION)
        Log::warn(TAG, "BASS version mismatch: library 0x" + std::to_string(ver) + ", header 0x" +
                           std::to_string(BASSVERSION));

    if (void* vm = g_javaVM.load()) BASS_SetConfigPtr(BASS_CONFIG_ANDROID_JAVAVM, vm);

    // Low-latency configuration. Must be set before BASS_Init.
    BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 5);  // ms between stream buffer updates
    BASS_SetConfig(BASS_CONFIG_BUFFER, 100);      // ms of stream playback buffer (affects seek/tempo response)
    BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, 10);   // ms of device output buffer

    if (!BASS_Init(-1, SAMPLE_RATE, 0, nullptr, nullptr)) {
        const int err = BASS_ErrorGetCode();
        if (err != BASS_ERROR_ALREADY) {
            Log::error(TAG, "BASS_Init failed, error code " + std::to_string(err));
            return false;
        }
    }
    g_initialized = true;
    Log::info(TAG, "BASS initialised (BASS_FX " + std::to_string(BASS_FX_GetVersion()) + ")");
    return true;
}

void BassAudio::free() {
    {
        std::lock_guard<std::mutex> lk(g_initMutex);
        if (!g_initialized) return;
        BASS_Free();
        g_initialized = false;
    }
    std::lock_guard<std::mutex> lk(g_sinkMutex);
    g_sinks.clear();
}

bool BassAudio::isInitialized() {
    std::lock_guard<std::mutex> lk(g_initMutex);
    return g_initialized;
}

// ---------------------------------------------------------------------------------------------------
// output latency
// ---------------------------------------------------------------------------------------------------

void BassAudio::setPollOutputLatency(bool enabled) {
    g_pollLatency.store(enabled);
    if (!enabled) g_outputLatencyMs.store(0.f);
}

float BassAudio::getOutputLatencyMs() {
    if (!g_pollLatency.load(std::memory_order_relaxed)) return 0.f;
    const int64_t now = nowMs();
    const int64_t last = g_lastLatencyPoll.load();
    // `last == INT64_MIN` = never polled. (Java used `now - Long.MIN_VALUE >= 1000`, which overflows to a
    // negative number and is never true, so it never polled. See docs/agents/T2.md.)
    if (last == INT64_MIN || now - last >= LATENCY_POLL_MS) {
        g_lastLatencyPoll.store(now);
        BASS_INFO info;
        if (BASS_GetInfo(&info)) {
            const float l = static_cast<float>(info.latency);  // DWORD, so already >= 0 (Java: Math.max(0, ...))
            const float old = g_outputLatencyMs.load();
            if ((l > old ? l - old : old - l) >= 5.f) Log::info(TAG, "Output latency: " + std::to_string(l) + " ms");
            g_outputLatencyMs.store(l);
        }
    }
    return g_outputLatencyMs.load();
}

// ---------------------------------------------------------------------------------------------------
// BASS thread -> game thread
// ---------------------------------------------------------------------------------------------------

void BassAudio::setEventPoster(EventPoster poster) {
    std::lock_guard<std::mutex> lk(g_eventMutex);
    g_poster = std::move(poster);
}

void BassAudio::postEvent(Event e) {
    EventPoster poster;
    {
        std::lock_guard<std::mutex> lk(g_eventMutex);
        if (!g_poster) {
            g_events.push_back(std::move(e));
            return;
        }
        poster = g_poster;
    }
    poster(std::move(e));
}

size_t BassAudio::pumpEvents() {
    std::vector<Event> batch;
    {
        std::lock_guard<std::mutex> lk(g_eventMutex);
        batch.swap(g_events);
    }
    for (auto& e : batch) {
        try {
            e();
        } catch (const std::exception& ex) {
            Log::warn(TAG, "audio event handler threw", ex);
        }
    }
    return batch.size();
}

std::shared_ptr<SyncSink> BassAudio::attachEndSync(uint32_t handle, std::function<void()> onGameThread) {
    auto sink = std::make_shared<SyncSink>();
    sink->onGameThread = std::move(onGameThread);
    sink->id = g_nextSinkId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lk(g_sinkMutex);
        g_sinks[sink->id] = sink;
    }
    // Same as Java: BASS_SYNC_END, param 0, not one-time, not mixtime (so it runs at playback time).
    if (!BASS_ChannelSetSync(handle, BASS_SYNC_END, 0, &endSyncProc, reinterpret_cast<void*>(sink->id))) {
        std::lock_guard<std::mutex> lk(g_sinkMutex);
        g_sinks.erase(sink->id);
        return nullptr;
    }
    return sink;
}

void BassAudio::releaseSink(std::shared_ptr<SyncSink>& sink) {
    if (!sink) return;
    {
        std::lock_guard<std::mutex> lk(g_sinkMutex);
        g_sinks.erase(sink->id);
    }
    sink->onGameThread = nullptr;  // events that are already queued become no-ops
    sink.reset();
}

// ---------------------------------------------------------------------------------------------------
// resources
// ---------------------------------------------------------------------------------------------------

void BassAudio::setAssetManager(AAssetManager* mgr) { g_assets.store(mgr); }

bool BassAudio::isPlainFile(const std::string& path) {
    struct stat st;
    return !path.empty() && ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && ::access(path.c_str(), R_OK) == 0;
}

bool BassAudio::readResource(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();
    if (path.empty()) return false;

    if (isPlainFile(path)) {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (f) {
            uint8_t buf[64 * 1024];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
            const bool ok = !std::ferror(f);
            std::fclose(f);
            if (ok) return true;
            out.clear();
        }
    }

#ifdef __ANDROID__
    // Not a file on disk: a path relative to the APK's assets/ folder (e.g. "res/theme.mp3").
    AAssetManager* mgr = g_assets.load();
    if (mgr && path[0] != '/') {
        AAsset* a = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_STREAMING);
        if (a) {
            uint8_t buf[64 * 1024];
            int n;
            while ((n = AAsset_read(a, buf, sizeof buf)) > 0) out.insert(out.end(), buf, buf + n);
            AAsset_close(a);
            if (n >= 0) return true;
            out.clear();
        }
    }
#endif
    return false;
}

// ---------------------------------------------------------------------------------------------------
// BassBackend.java
// ---------------------------------------------------------------------------------------------------

BassBackend::BassBackend() { BassAudio::init(); }

std::unique_ptr<AbsMusic> BassBackend::createMusic(const std::string& path) {
    return std::unique_ptr<AbsMusic>(new BassMusic(path));  // throws java::IOException on failure
}

std::unique_ptr<ClipImplementation> BassBackend::createClip(const std::string& path) {
    return BassClip::load(path);
}

std::unique_ptr<ClipImplementation> BassBackend::createStreamClip(const std::string& url) {
    return std::unique_ptr<ClipImplementation>(new BassStreamClip(url));
}

// ---------------------------------------------------------------------------------------------------
// debug entry point
// ---------------------------------------------------------------------------------------------------

std::unique_ptr<AbsMusic> debugPlayAsset(AAssetManager* assets, const std::string& assetPath, float volume,
                                         bool loop) {
    BassAudio::setAssetManager(assets);
    if (!BassAudio::init()) return nullptr;
    try {
        BassBackend backend;
        std::unique_ptr<AbsMusic> music = backend.createMusic(assetPath);
        music->setVolume(volume);
        if (loop)
            music->loop();
        else
            music->play();
        Log::info("BassDebug", "playing " + assetPath);
        return music;
    } catch (const java::IOException& e) {
        Log::error("BassDebug", "could not play " + assetPath, e);
        return nullptr;
    }
}

}  // namespace bass
}  // namespace opsu
