#pragma once
#include <functional>
#include <string>

namespace opsu {

/**
 * One streaming music track. Port of fluddokt.opsu.fake.AbsMusic.
 * Created by IAudioBackend::createMusic(). Positions are in SECONDS (float), like Java.
 *
 * Threading: the game calls everything on the game thread. A backend that finishes a track or wants
 * a resync on its own thread (BASS sync callback) must marshal onComplete / onRequestSync to the game
 * thread (or at least never run game logic on the audio thread).
 *
 * Lead-in (negative positions) is NOT the backend's job: the Music wrapper in MusicController.h holds
 * the backend at 0 and runs a virtual clock. setPosition() may clamp negative values to 0.
 */
class AbsMusic {
public:
    virtual ~AbsMusic() = default;

    /** Seeks to `sec`. Returns false if the seek failed. */
    virtual bool setPosition(float sec) = 0;
    /** Starts playing, looping at the end. */
    virtual void loop() = 0;
    /** Starts playing from the current position. */
    virtual void play() = 0;
    virtual void pause() = 0;
    /** Continues after pause(). */
    virtual void resume() = 0;
    /** Stops playback (position is implementation-defined afterwards; Java Silent = like pause()). */
    virtual void stop() = 0;
    /** Releases the native resources. The Music wrapper guarantees it is called at most once. */
    virtual void dispose() = 0;

    virtual bool playing() = 0;
    /** Current position in seconds (already pitch-scaled track time, i.e. position inside the file). */
    virtual float getPosition() = 0;
    /** Source path / name of the track. */
    virtual std::string getName() = 0;

    /** Volume [0, 1]. */
    virtual void setVolume(float volume) = 0;
    /** Playback speed ratio (1 = normal; also changes pitch, like the Java backends). */
    virtual void setPitch(float pitch) = 0;

    /** Java default: volFade(duration, f, b) just pauses. */
    virtual void volFade(int /*durationMs*/, float /*endVolume*/, bool /*endAfterFade*/) { pause(); }
    /** Java default: no-op. */
    virtual void pitchFade(int /*durationMs*/, float /*endPitch*/) {}

    /** Fills out[0..255] with FFT magnitudes 0..1 (BASS_DATA_FFT512: 256 bins, bin width = sampleRate/512).
     *  Returns false if unavailable right now (out untouched). */
    virtual bool getFFT(float* /*out256*/) { return false; }
    /** Fills out[0] = left peak, out[1] = right peak (0..1). Returns false if unavailable. */
    virtual bool getLevels(float /*out*/[2]) { return false; }
    /** Low-pass filter ("muffle"). cutoffHz >= 20000 (MusicController::LOWPASS_OFF) = remove the filter;
     *  otherwise create the effect once and only update its parameters later (called every frame). */
    virtual void setLowPass(float /*cutoffHz*/) {}

    /** Fire when the track reaches its end (not when stopped/looped). Set by the Music wrapper. */
    std::function<void(AbsMusic&)> onComplete;
    /** Java AbsMusicCompleteListener.requestSync: the backend asks the Music wrapper to re-sync its
     *  smoothed position clock (e.g. after a latency change / seek it did on its own). Optional. */
    std::function<void(AbsMusic&)> onRequestSync;

protected:
    /** Java fireMusicEnded(). */
    void fireMusicEnded() { if (onComplete) onComplete(*this); }
    /** Java listener.requestSync(this). */
    void fireRequestSync() { if (onRequestSync) onRequestSync(*this); }
};

}  // namespace opsu
