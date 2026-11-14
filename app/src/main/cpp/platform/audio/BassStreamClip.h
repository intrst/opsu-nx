#pragma once
// T2: one-shot clip streamed from a URL (beatmap previews). Port of BassStreamClip.java (does not include bass.h).
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "BassBackend.h"
#include "opsu/audio/ClipImplementation.h"

namespace opsu {
namespace bass {

/**
 * The stream is opened on play(). NOTE: BASS_StreamCreateURL blocks the calling thread while it connects
 * (BASS's net timeout, 5 s by default); Java behaved the same.
 *
 * `onEnd` runs when the stream reaches its end (BASS_SYNC_END), on the game thread via
 * BassAudio::pumpEvents(). Like Java it is NOT called when stop() / destroy() / a new play() ends the stream.
 */
class BassStreamClip : public ClipImplementation {
public:
    explicit BassStreamClip(const std::string& url) : url_(url) {}
    ~BassStreamClip() override;
    BassStreamClip(const BassStreamClip&) = delete;
    BassStreamClip& operator=(const BassStreamClip&) = delete;

    int play(float volume, std::function<void()> onEnd) override;
    void stop() override;
    void destroy() override;  // idempotent

private:
    void release();  // caller holds mutex_

    const std::string url_;
    std::mutex mutex_;
    uint32_t stream_ = 0;
    std::shared_ptr<SyncSink> sink_;
};

}  // namespace bass
}  // namespace opsu
