#pragma once
// T2: short sound effect (hit sounds, UI sounds). Port of BassClip.java (does not include bass.h).
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "opsu/audio/ClipImplementation.h"

namespace opsu {
namespace bass {

/**
 * The file is decoded fully into memory once (BASS_SampleLoad); each play grabs a fresh channel, so
 * overlapping hits don't cut each other off. A sample handle of 0 means "silent clip".
 *
 * Like the Java class, play() ignores `onEnd`: only BassStreamClip (beatmap previews) reports its end.
 */
class BassClip : public ClipImplementation {
public:
    /** Never null. If loading fails the clip is silent (logged); there is no fallback engine. */
    static std::unique_ptr<BassClip> load(const std::string& path);
    ~BassClip() override;
    BassClip(const BassClip&) = delete;
    BassClip& operator=(const BassClip&) = delete;

    void stop() override;
    int play(float volume, std::function<void()> onEnd) override;
    void destroy() override;  // idempotent

    int playLoop(float volume, float frequencyRatio) override;
    void setChannelVolume(int channel, float volume) override;
    void setChannelFrequency(int channel, float frequencyRatio) override;
    void stopChannel(int channel) override;

private:
    explicit BassClip(uint32_t sample) : sample_(sample) {}

    std::atomic<uint32_t> sample_;
    /** Original sample rate of loop channels whose frequency has been modulated, by channel. */
    std::mutex ratesMutex_;
    std::unordered_map<int, float> baseRates_;
};

}  // namespace bass
}  // namespace opsu
