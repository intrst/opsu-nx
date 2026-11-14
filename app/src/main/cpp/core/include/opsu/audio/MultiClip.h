#pragma once
#include <functional>
#include <memory>
#include <string>

#include "opsu/audio/ClipImplementation.h"

namespace opsu {

/**
 * A sound effect. Port of fluddokt.opsu.fake.Clip: all playback is delegated to the ClipImplementation the active
 * IAudioBackend creates. (Java has the wrapper in the `fake` package; it lives here because T1 owns no other file for it.)
 * The destructor destroys the implementation (ClipImplementation::destroy is idempotent), which Java left to the GC.
 */
class Clip {
public:
    /** audioBackend().createClip(path). Never has a null implementation (a failed load is a silent clip). */
    explicit Clip(const std::string& path);
    /** audioBackend().createStreamClip(url). Java's `LineListener` argument of this constructor is unused there, so it is not taken. */
    struct StreamTag {};
    Clip(const std::string& url, StreamTag);
    virtual ~Clip();
    Clip(const Clip&) = delete;
    Clip& operator=(const Clip&) = delete;

    /** Plays once. `onEnd` may be empty. */
    void start(float volume, std::function<void()> onEnd = nullptr);
    /** Channel id of the last start() call (0 if the backend could not play it). */
    int getLastPlayId() const { return id_; }

    /** Starts a looping playback; returns a loop id (0 = none) for the other loop methods. */
    int startLoop(float volume, float frequencyRatio);
    void setLoopVolume(int loopId, float volume);
    void setLoopFrequency(int loopId, float frequencyRatio);
    void stopLoop(int loopId);

    void destroy();
    void stop();

private:
    std::unique_ptr<ClipImplementation> sound_;
    int id_ = 0;
};

/**
 * fluddokt.opsu.fake.MultiClip: a Clip (the original itdelatrisu MultiClip, which played several copies of one clip,
 * is dead code in the Java port; the backend does the mixing now). Use std::shared_ptr<MultiClip>: the clip tables of
 * SoundController, the beatmap sample map and the running loops share clips.
 */
class MultiClip : public Clip {
public:
    explicit MultiClip(const std::string& path) : Clip(path) {}
    /** A one-shot clip streamed from a URL (beatmap previews). */
    MultiClip(const std::string& url, StreamTag tag) : Clip(url, tag) {}

    /** Java: empty. */
    static void destroyExtraClips() {}
};

}  // namespace opsu
