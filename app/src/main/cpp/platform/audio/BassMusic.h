#pragma once
// T2: streaming music track backed by BASS. Port of BassMusic.java (does not include bass.h).
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "BassBackend.h"
#include "opsu/audio/AbsMusic.h"

namespace opsu {
namespace bass {

/**
 * With BASS_FX the file is opened as a decode stream and wrapped in a tempo stream, so DT/HT change speed
 * without changing pitch. Positions BASS reports for a tempo stream are in source (track) time, which is what
 * the game expects from getPosition().
 *
 * Thread model: the game calls everything on the game thread. getFFT()/getLevels()/getPosition() only use
 * BASS calls, which are thread-safe, so a visualiser thread may call them too. onComplete is raised from
 * BassAudio::pumpEvents() on the game thread, never on BASS's thread.
 */
class BassMusic : public AbsMusic {
public:
    /** Throws java::IOException if the file cannot be opened or decoded (same messages as Java). */
    explicit BassMusic(const std::string& path);
    ~BassMusic() override;
    BassMusic(const BassMusic&) = delete;
    BassMusic& operator=(const BassMusic&) = delete;

    bool setPosition(float seconds) override;
    void loop() override;
    void play() override;
    bool playing() override;
    void pause() override;
    void resume() override;
    void stop() override;  // like Java: halts output but keeps the position (BASS_ChannelPause)
    void dispose() override;
    std::string getName() override { return path_; }
    float getPosition() override;
    void setVolume(float volume) override;
    void setPitch(float pitch) override;
    bool getFFT(float* out256) override;
    bool getLevels(float out[2]) override;
    void setLowPass(float cutoffHz) override;

    /** The rate last applied through setPitch(). */
    float getRate() const { return rate_.load(); }

private:
    void restartIfEnded();
    void setLoop(bool loop);

    const std::string path_;
    /** Handle used for all playback calls (the BASS_FX tempo stream). 0 once disposed. */
    std::atomic<uint32_t> stream_{0};
    float baseFreq_ = 44100.f;
    /** Keeps the in-memory copy alive for as long as the stream may read from it. */
    std::vector<uint8_t> memory_;
    /** BASS_SYNC_END link; `fired` is Java's `ended` flag. */
    std::shared_ptr<SyncSink> endSink_;
    std::atomic<float> rate_{1.f};

    // muffle (setLowPass): the BASS_FX biquad effect handle, created once and then only re-parameterised
    uint32_t lowPassFx_ = 0;
    float lowPassHz_ = 0.f;
    bool lowPassWarned_ = false;
};

}  // namespace bass
}  // namespace opsu
