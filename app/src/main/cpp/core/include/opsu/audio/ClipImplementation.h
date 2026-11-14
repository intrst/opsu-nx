#pragma once
#include <functional>

namespace opsu {

/**
 * One short, fully loaded sound effect (or a one-shot streamed preview).
 * Port of fluddokt.opsu.fake.ClipImplementation. Created by IAudioBackend::createClip /
 * createStreamClip. A failed load must give a silent clip, never null.
 *
 * Channel ids are backend-defined non-zero ints; 0 always means "nothing playing".
 * Callbacks (`onEnd`) may be invoked from the audio thread; the caller (Clip / SoundController)
 * must tolerate that, a backend should marshal to the game thread where it can.
 */
class ClipImplementation {
public:
    virtual ~ClipImplementation() = default;

    /** Stops every channel of this clip. */
    virtual void stop() = 0;
    /** Plays once at volume [0, 1]. `onEnd` (may be empty) is called when playback ends or the
     *  clip is stopped. Returns the channel id, or 0 if nothing is playing. */
    virtual int play(float volume, std::function<void()> onEnd) = 0;
    /** Releases the native resources (idempotent). */
    virtual void destroy() = 0;

    /** Starts a looping playback on a new channel (slider slide/whistle, spinner spin).
     *  frequencyRatio: playback speed relative to the sample's own rate (1 = unchanged).
     *  Returns a channel id for the three methods below, or 0 if nothing is playing. */
    virtual int playLoop(float /*volume*/, float /*frequencyRatio*/) { return 0; }
    /** Volume [0, 1] of a channel started with playLoop(). */
    virtual void setChannelVolume(int /*channel*/, float /*volume*/) {}
    /** Playback speed ratio of a channel started with playLoop(). */
    virtual void setChannelFrequency(int /*channel*/, float /*frequencyRatio*/) {}
    /** Stops a single channel started with playLoop(). */
    virtual void stopChannel(int /*channel*/) {}
};

}  // namespace opsu
