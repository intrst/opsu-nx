#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "opsu/audio/HitSound.h"
#include "opsu/audio/MultiClip.h"
#include "opsu/audio/SoundEffect.h"
#include "opsu/beatmap/HitObject.h"

namespace opsu {

/**
 * Controller for all (non-music) sound components. Port of itdelatrisu.opsu.audio.SoundController (static class, like Java).
 *
 * SoundComponent: the Java interface (`getClip()`) is implemented by the enums SoundEffect and HitSound. Those enums are
 * plain enums in C++ (see SoundEffect.h / HitSound.h), so the controller owns the clip tables and the overloads taking a
 * SoundEffect / HitSound replace `playSound(SoundComponent)`. getClip(HitSound) is Java `HitSound.getClip()`
 * (the clip of the sample set chosen with hitsound::setSampleSet, null until then).
 *
 * Threading: game thread, except that the `onEnd` listener of a clip may be called by the backend. The set of running
 * loops and playTrack/stopTrack are mutex-guarded like Java's synchronized.
 */
class SoundController {
public:
    /** What the Java class reads from Options / Skin / UI / ErrorHandler / ResourceLoader / File. Defaults: all volumes 1, sound enabled, no skin. */
    struct Host {
        /** Options.getEffectVolume() / getHitSoundVolume() / getMasterVolume() / getMusicVolume() ([0, 1]). */
        std::function<float()> effectVolume = [] { return 1.f; };
        std::function<float()> hitSoundVolume = [] { return 1.f; };
        std::function<float()> masterVolume = [] { return 1.f; };
        std::function<float()> musicVolume = [] { return 1.f; };
        /** Options.isSoundDisabled() / isGameplaySoundDisabled(). */
        std::function<bool()> soundDisabled = [] { return false; };
        std::function<bool()> gameplaySoundDisabled = [] { return false; };
        /** Options.getSkin().getDirectory(); "" = none (Java null). */
        std::function<std::string()> skinDirectory = [] { return std::string(); };
        /** Options.getSkin().isLayeredHitSounds(). */
        std::function<bool()> layeredHitSounds = [] { return true; };
        /** ResourceLoader.resourceExists(name) (the bundled default sounds, e.g. "normal-hitnormal.wav"). Default: false. */
        std::function<bool(const std::string&)> resourceExists;
        /** File.isFile() / File.isDirectory() / dir.listFiles() (file NAMES, not paths; empty if unreadable). Default: std::filesystem. */
        std::function<bool(const std::string&)> isFile;
        std::function<bool(const std::string&)> isDirectory;
        std::function<std::vector<std::string>(const std::string&)> listFiles;
        /** UI.getNotificationManager().sendNotification(text, Color.red) / sendBarNotification(text). Default: Log::warn / Log::info. */
        std::function<void(const std::string&)> redNotification;
        std::function<void(const std::string&)> barNotification;
        /** ErrorHandler.error(message, e, report). `e` may be null. Default: Log::error. */
        std::function<void(const std::string&, const std::exception*, bool)> error;
    };
    /** Installs the seams (members left empty keep their default). */
    static void setHost(Host host);
    static const Host& host();

    // --- clip tables (SoundEffect.getClip/setClip, HitSound.getClip/setClip) ---
    static std::shared_ptr<MultiClip> getClip(SoundEffect e);
    static void setClip(SoundEffect e, std::shared_ptr<MultiClip> clip);
    static std::shared_ptr<MultiClip> getClip(HitSound s, SampleSet set);
    static void setClip(HitSound s, SampleSet set, std::shared_ptr<MultiClip> clip);
    /** Java HitSound.getClip(): the clip of the active sample set (hitsound::activeSampleSet()), null if none is active. */
    static std::shared_ptr<MultiClip> getClip(HitSound s);

    /** Loads all sound files. */
    static void init();
    /** Sets the sample volume (global multiplier from timing points) [0, 1]; out-of-range values are ignored. */
    static void setSampleVolume(float volume);

    // --- one-shot sounds ---
    static void playSound(SoundEffect s);
    static void playSound(HitSound s);
    /** Plays and reports whether the backend actually started it (true if muted/unavailable, so retrying is pointless). */
    static bool playSoundChecked(SoundEffect s);
    static bool playSoundChecked(HitSound s);
    static void stopSound(SoundEffect s);
    static void stopSound(HitSound s);

    /** Hit sound(s) of a hit object (or one of a slider's edges), the osu!lazer way. edgeIndex: 0 = head ... repeats + 1 = tail; 0 for others. */
    static void playHitSound(const HitObject& hitObject, int edgeIndex);
    static void playSliderTick(const HitObject& hitObject);
    /** Java playHitSound(SoundComponent). */
    static void playHitSound(HitSound s);
    /** Custom sample index of the current timing point (0 = none, 1 = beatmap's default files, 2+ = suffixed files). */
    static void setTimingPointCustomIndex(int index);

    static void preloadIntroSounds();
    static void playIntroHit(bool finish);

    // --- looping sounds ---
    /** A single looping playback. Stops itself when destroyed. */
    class Loop {
    public:
        Loop() = default;
        ~Loop();
        Loop(const Loop&) = delete;
        Loop& operator=(const Loop&) = delete;
        /** Starts the loop (no-op if already playing, muted, or volume <= 0). */
        void start(std::shared_ptr<MultiClip> clip, float volume, float frequencyRatio);
        bool isPlaying() const { return id_ != 0; }
        void setVolume(float volume);
        void setFrequency(float ratio);
        /** Stops the loop (no-op if not playing). */
        void stop();

    private:
        std::shared_ptr<MultiClip> clip_;
        int id_ = 0;
    };

    /** The sliding sounds of a slider ("sliderslide" + "sliderwhistle" loops). `hitObject` must outlive this object. */
    class SlidingSound {
    public:
        explicit SlidingSound(const HitObject& hitObject) : hitObject_(&hitObject) {}
        void start();
        void stop();

    private:
        const HitObject* hitObject_;
        Loop slide_, whistle_;
    };

    static std::shared_ptr<MultiClip> getSpinnerSpinClip();
    static float getLoopVolume(const HitObject& hitObject);
    static float getEffectLoopVolume();
    /** Stops every looping sound (slider sliding, spinner spinning); called when the game is paused, restarted, left or seeked. */
    static void stopAllLoops();

    // --- beatmap-specific samples ---
    /**
     * Loads the hit samples shipped inside a beatmap's folder (e.g. "soft-hitnormal.wav", "normal-hitclap2.wav") plus any sample
     * file hit objects reference by name; replaces the previously loaded ones. Files are visited in name order (Java: directory
     * order). Names are lower-cased with ASCII rules.
     * @param dir the beatmap folder ("" = just clear)
     * @param referencedNames lower-case sample names (no extension) referenced by hit objects, or null
     */
    static void loadBeatmapSamples(const std::string& dir, const std::set<std::string>* referencedNames);
    static void clearBeatmapSamples();

    static void mute(bool mute);
    /** Name of the file being loaded, or nullopt. */
    static std::optional<std::string> getCurrentFileName();
    /** Loading progress [0, 100], or -1 if not loading. */
    static int getLoadingProgress();

    /**
     * Plays a track from a remote URL (beatmap preview); a track already playing is stopped. `filename` is unused (Java too).
     * @return true if playing; false (+ bar notification "Failed to load clip '<url>'.") if the URL has no protocol (Java: MalformedURLException)
     */
    static bool playTrack(const std::string& url, const std::string& filename, std::function<void()> listener);
    static void stopTrack();

    /** Restores the initial static state without touching the host (for tests): clips destroyed, mute off, sample volume 1. */
    static void resetForTesting();

private:
    SoundController() = delete;
};

}  // namespace opsu
