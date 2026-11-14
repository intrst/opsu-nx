#pragma once
#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "opsu/audio/AbsMusic.h"
#include "opsu/beatmap/Beatmap.h"
#include "opsu/beatmap/TimingPoint.h"

namespace opsu {

/**
 * fluddokt.opsu.fake.Music (+ the SoundStore bits the controller uses): the game-side wrapper around an AbsMusic.
 * It adds (1) the lead-in for negative positions (the backend is held at 0 while a virtual clock counts up to 0),
 * (2) the smoothed getPosition() (a clock that follows the backend position, re-synced when it drifts > 200 ms),
 * (3) fades (volume / pitch).
 *
 * Differences from Java, all on purpose:
 *  - Java fade()/pitchFade() each start a thread that sleeps 10 ms between steps. Here they are ticked from the game
 *    thread by updateFades() (MusicController::updateMuffle calls it every frame), so nothing audio-related runs on a
 *    second thread and tests are deterministic. A new fade replaces a running one (Java ran both threads at once).
 *    dispose() cancels fades (Java's thread went on calling pause() on the released track).
 *  - Java's pitchFade stops at the first step past `duration` without applying it, so the last applied pitch is the
 *    last step before the end; that is kept. Java's volume fade does apply that overshooting last step; here the final
 *    step is exactly the end value.
 *  - `Music::setVolume(float)` is an EMPTY method in Java (only setMusicVolume changes the backend volume), and fade()
 *    calls it, so a Java volume "fade" does nothing audible except the pause at its end. That is kept as it is
 *    (see docs/agents/T1.md); the one-word fix is in Music::updateFades.
 *  - Java's GameContainer.setMusic(this) is not ported (no GameContainer in core). SoundStore.get().soundWorks() is always true there.
 *
 * Create with Music::create() (registers it as the "SoundStore" music, like the Java constructor does).
 */
class Music {
public:
    using Listener = std::function<void(Music&)>;  // Java MusicListener.musicEnded (musicSwapped was a no-op)

    /** audioBackend().createMusic(path). On failure logs and throws std::runtime_error("Could not load music: <path>"). */
    static std::shared_ptr<Music> create(const std::string& path);
    /** Wraps an existing track (tests). Also registers it as the SoundStore music. */
    static std::shared_ptr<Music> wrap(std::unique_ptr<AbsMusic> music);
    ~Music();
    Music(const Music&) = delete;
    Music& operator=(const Music&) = delete;

    bool setPosition(float f);
    void loop();
    void play();
    bool playing();
    void pause();
    void resume();
    /** Java: empty (see class comment). */
    void setVolume(float) {}
    void stop();
    float getPosition();
    /** Releases the backend track (at most once). */
    void dispose();
    /** Sets the backend volume. */
    void setMusicVolume(float musvolume);
    void addListener(Listener l) { listeners_.push_back(std::move(l)); }
    bool getFFT(float* out256);
    bool getLevels(float out[2]);
    void setLowPass(float cutoffHz);
    void setPitch(float pitch);
    void pitchFade(int duration, float endPitch);
    void fade(int duration, float endVolume, bool endAfterFade);
    /** Advances running fades to the current audioMillis(). */
    void updateFades();

    float getPitch() const { return pitch_; }
    float getVolume() const { return volume_; }
    /** The backend track (tests). */
    AbsMusic* backend() { return music_.get(); }

    // SoundStore (static, one "current" music: the last one created)
    static void soundStoreSetMusicVolume(float musicVolume);
    static void soundStoreSetMusicPitch(float pitch);

private:
    explicit Music(std::unique_ptr<AbsMusic> music);
    void complete();
    void syncPosition();
    float leadInCurrent();
    void leadInRebase();
    void leadInCancel();

    std::unique_ptr<AbsMusic> music_;
    std::vector<Listener> listeners_;
    float pitch_ = 1.f;
    float volume_ = 1.f;

    bool leadInActive_ = false;   // positioned before the start of the track (backend held at 0)
    bool leadInRunning_ = false;  // the virtual lead-in clock is ticking
    float leadInPos_ = 0.f;       // virtual position (s, negative) at leadInStamp_
    int64_t leadInStamp_ = 0;     // time (ms) leadInPos_ was last updated

    float lastPosition_ = 0.f;
    float lastUpdatePosition_ = 0.f;
    int64_t lastTime_ = 0;
    int64_t lastUpdateTime_ = 0;
    float avgDiff_ = 0.f;
    int diffCnt_ = 0;

    bool disposed_ = false;
    std::mutex disposeMutex_;

    struct Fade {
        bool active = false;
        int64_t start = 0;
        float from = 0.f, to = 0.f;
        int duration = 0;
        bool endAfterFade = false;
    };
    Fade volumeFade_, pitchFade_;
};

/**
 * Controller for all music. Port of itdelatrisu.opsu.audio.MusicController (static class, like Java).
 *
 * Seams (Java statics this port cannot reach): see Host. Everything else, including the muffle logic, is the Java one.
 * Threading: call everything from the game thread. The AbsMusic end-of-track callback must be marshalled there by the backend.
 * Lifetime: the controller keeps a pointer to the beatmap given to play() (like Java keeps the reference): call reset()
 * (or play() another beatmap) before that Beatmap is destroyed.
 *
 * Removed from Java: the `trackLoader` thread is commented out there (tracks load synchronously), so isTrackLoading() is
 * always false; the Utils.gc() background thread and destroyOpenAL's OpenAL internals are dropped (the backend frees the track).
 */
class MusicController {
public:
    /** Cutoff (Hz) at and above which the low-pass filter counts as off. */
    static constexpr float LOWPASS_OFF = 20000.f;

    /** What the Java class reads from Options / UI / ErrorHandler / ResourceLoader. Defaults: volumes 1, offset 0, no theme. */
    struct Host {
        /** Options.getMusicVolume() / getMasterVolume() ([0, 1]). */
        std::function<float()> musicVolume = [] { return 1.f; };
        std::function<float()> masterVolume = [] { return 1.f; };
        /** Options.getMusicOffset() (ms). */
        std::function<int()> musicOffset = [] { return 0; };
        /** Options.getThemeBeatmap(); null = none. */
        std::function<const Beatmap*()> themeBeatmap = [] { return static_cast<const Beatmap*>(nullptr); };
        /** `file.isFile() || ResourceLoader.resourceExists(path)`. Default: a regular file on disk. */
        std::function<bool(const std::string&)> trackExists;
        /** UI.getNotificationManager().sendBarNotification(text). Default: Log::info. */
        std::function<void(const std::string&)> barNotification;
        /** ErrorHandler.error(message, e, report). `e` may be null. Default: Log::error. */
        std::function<void(const std::string&, const std::exception*, bool)> error;
    };
    /** Installs the seams (members left empty keep their default). */
    static void setHost(Host host);
    static const Host& host();

    /** The current track (null if none). */
    static std::shared_ptr<Music> getPlayer();

    // --- muffle (low-pass), see MUFFLE_NOTES.txt ---
    static void setMuffled(bool muffled);
    static void onVolumeAdjusted();
    /** Advances the muffle animation and the fades of the track. Call every frame (every state, also during transitions). */
    static void updateMuffle(int delta);
    static void setStartVolumeScale(float scale);

    // --- playback ---
    static void play(const Beatmap& beatmap, bool loop, bool preview);
    static void playAt(int position, bool loop);
    static void play(bool loop);
    static bool isTrackLoading() { return false; }
    static bool trackExists();
    static bool getLevels(float out[2]);
    static bool getFFT(float* out256);
    static const Beatmap* getBeatmap();

    // --- timing ---
    /** Beat progress [0, 1); with no timing information 60 bpm by the clock. */
    static float getBeatProgress();
    static TimingPoint getLastNonInheritedTimingPoint();
    static TimingPoint getLastTimingPoint();
    /** Measure progress [0, 1), or nullopt if no timing information is available (music paused, no timing points). */
    static std::optional<float> getMeasureProgress(int k = 1);

    static bool isPlaying();
    static bool isPaused();
    static void pause();
    static void resume();
    static void stop();
    static void fadeOut(int duration);
    static void pitchFadeOut(int duration);

    /** Position in the track in ms (0 if none); with offsets: Options music offset + beatmap local offset. */
    static int getPosition(bool useOffsets);
    static int getTotalOffset();
    static bool setTrackPosition(int trackPosition);
    /** Seeks to a raw position (no offsets; negative = lead-in). */
    static bool setPosition(int position);
    static int getDuration();

    static void setVolume(float volume);
    static void setPitch(float pitch);
    static bool trackEnded();
    static void loopTrackIfEnded(bool preview);
    static void playThemeSong();
    static bool isThemePlaying();
    static bool isTrackDimmed();
    static void toggleTrackDimmed(float multiplier);
    /** Completely resets the controller state (stops and releases the track). */
    static void reset();

private:
    MusicController() = delete;
};

}  // namespace opsu
