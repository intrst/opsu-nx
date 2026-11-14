#pragma once
// T2: BASS audio backend (Android only). Port of fluddokt.opsu.android.bass.{BassBackend,BassAudio}.
//
// Everything BASS-specific stays in platform/audio/*.cpp; this header deliberately does NOT include
// bass.h, so other code can include it freely.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "opsu/audio/IAudioBackend.h"

struct AAssetManager;  // <android/asset_manager.h>; only forward-declared here

namespace opsu {
namespace bass {

/** Volume clamp used by every BASS wrapper: [0, 1]; NaN -> 0 (Java would pass NaN to BASS). */
inline float clampVolume(float v) {
    if (!(v > 0.f)) return 0.f;
    return v > 1.f ? 1.f : v;
}

/**
 * Link between one BASS sync callback (runs on BASS's own thread) and its owner (game thread).
 * The BASS thread only sets `fired` and posts an event; `onGameThread` is only ever touched on the game
 * thread (set before the sync is installed, cleared by BassAudio::releaseSink()).
 */
struct SyncSink {
    std::atomic<bool> fired{false};
    std::function<void()> onGameThread;
    uintptr_t id = 0;
};

/**
 * Global BASS lifecycle + helpers shared by BassMusic / BassClip / BassStreamClip.
 * Port of BassAudio.java (init / free / output latency), plus the game-thread event queue.
 */
class BassAudio {
public:
    // ---- lifecycle -----------------------------------------------------------------------------
    /** Optional, call BEFORE init(): hands BASS the JavaVM (BASS_SetConfigPtr(BASS_CONFIG_ANDROID_JAVAVM)).
     *  The Java build got it automatically because System.loadLibrary("bass") runs libbass's JNI_OnLoad; a
     *  native-only app does not. Only needed if Android-Media-codec formats or file descriptors fail on a device
     *  (NativeActivity: app->activity->vm). Harmless to skip for plain mp3/ogg/wav. */
    static void setJavaVM(void* javaVM);
    /** BASS_SetConfig (latency) + BASS_Init(-1, 44100, 0). Idempotent. Returns false (and logs) on failure;
     *  Java treated that as fatal, here the caller decides (BassBackend then gives silent clips / throws on music). */
    static bool init();
    /** BASS_Free: releases the device and every stream/sample created on it. */
    static void free();
    static bool isInitialized();

    // ---- output latency ------------------------------------------------------------------------
    /** Output-device latency in ms (BASS_INFO.latency, re-read every second) when polling is enabled,
     *  otherwise 0. See setPollOutputLatency() and docs/agents/T2.md ("Java latency bug"). */
    static float getOutputLatencyMs();
    /** Default false, which is what the Java build did in practice (its poll condition overflowed and
     *  never ran). true = poll BASS_INFO.latency as the Java comments intended. Thread-safe. */
    static void setPollOutputLatency(bool enabled);

    // ---- BASS thread -> game thread -------------------------------------------------------------
    using Event = std::function<void()>;
    using EventPoster = std::function<void(Event)>;
    /** Call once per frame on the GAME thread: runs every event the BASS thread queued (music ended,
     *  preview clip ended). Returns how many events ran. Without this call (or a poster) onComplete never fires. */
    static size_t pumpEvents();
    /** Optional: route events to your own game-thread mechanism instead of the built-in queue. The poster is
     *  called on BASS's thread, so it must be thread-safe. nullptr restores the built-in queue. */
    static void setEventPoster(EventPoster poster);
    /** Queue an event for the game thread (any thread). */
    static void postEvent(Event e);

    // ---- BASS_SYNC_END helper ------------------------------------------------------------------
    /** Installs BASS_SYNC_END on `handle`. When it fires, `fired` is set at once (BASS thread) and
     *  `onGameThread` runs from pumpEvents(). Returns nullptr if BASS_ChannelSetSync failed. */
    static std::shared_ptr<SyncSink> attachEndSync(uint32_t handle, std::function<void()> onGameThread);
    /** Game thread: detach `sink` (late events become no-ops) and reset the pointer. Safe on null. */
    static void releaseSink(std::shared_ptr<SyncSink>& sink);

    // ---- resources -----------------------------------------------------------------------------
    /** The asset manager used for relative paths that are not files on disk (e.g. "res/theme.mp3").
     *  Not owned; keep it alive while audio is used. (NativeActivity: app->activity->assetManager.) */
    static void setAssetManager(AAssetManager* mgr);
    /** true if `path` is a readable regular file on the file system. */
    static bool isPlainFile(const std::string& path);
    /** Reads a whole resource: first the file system, then (Android) the asset manager for relative paths.
     *  Returns false if not found. An existing empty file returns true with out.empty(). */
    static bool readResource(const std::string& path, std::vector<uint8_t>& out);
};

/** The game's only audio engine on Android (port of BassBackend.java). Stateless: BASS state is global. */
class BassBackend : public IAudioBackend {
public:
    /** Calls BassAudio::init(). */
    BassBackend();
    ~BassBackend() override = default;

    /** Throws java::IOException (opsu/util/JavaIO.h) if the file cannot be opened/decoded or BASS is not up. */
    std::unique_ptr<AbsMusic> createMusic(const std::string& path) override;
    /** Never null: a failed load is a silent clip (logged). */
    std::unique_ptr<ClipImplementation> createClip(const std::string& path) override;
    /** Never null. The stream is opened on play(). */
    std::unique_ptr<ClipImplementation> createStreamClip(const std::string& url) override;
};

/**
 * Debug entry point for the "done when" check: sets the asset manager, inits BASS and starts
 * `assetPath` (default "res/theme.mp3" from assets/) at `volume`, looped. Returns the playing music
 * (keep it alive; destroy it or call dispose() to stop) or nullptr on failure (logged).
 * Does not touch the global audio-backend registry, so it works before T1's .cpp files are merged.
 * You must also call BassAudio::pumpEvents() each frame if you want onComplete events.
 *
 *   // android_main(android_app* app):
 *   auto music = opsu::bass::debugPlayAsset(app->activity->assetManager);
 */
std::unique_ptr<AbsMusic> debugPlayAsset(AAssetManager* assets, const std::string& assetPath = "res/theme.mp3",
                                         float volume = 0.5f, bool loop = true);

}  // namespace bass
}  // namespace opsu
