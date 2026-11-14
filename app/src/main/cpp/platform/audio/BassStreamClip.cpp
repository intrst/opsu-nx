// T2: one-shot clip streamed from a URL (beatmap previews). Port of BassStreamClip.java.
#include "BassStreamClip.h"

#include "../../third_party/bass/bass.h"

#include "opsu/logging/Log.h"

namespace opsu {
namespace bass {

namespace {
const char* const TAG = "BassStreamClip";
}

BassStreamClip::~BassStreamClip() { destroy(); }

int BassStreamClip::play(float volume, std::function<void()> onEnd) {
    std::lock_guard<std::mutex> lk(mutex_);
    release();
    stream_ = BASS_StreamCreateURL(url_.c_str(), 0, 0, nullptr, nullptr);
    if (stream_ == 0) {
        Log::warn(TAG, "BASS_StreamCreateURL failed for " + url_ + " (error " + std::to_string(BASS_ErrorGetCode()) + ")");
        return 0;
    }
    BASS_ChannelSetAttribute(stream_, BASS_ATTRIB_VOL, clampVolume(volume));
    if (onEnd) sink_ = BassAudio::attachEndSync(stream_, std::move(onEnd));  // BASS thread -> game thread
    BASS_ChannelPlay(stream_, FALSE);
    return static_cast<int>(stream_);
}

void BassStreamClip::stop() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (stream_ != 0) BASS_ChannelStop(stream_);
}

void BassStreamClip::destroy() {
    std::lock_guard<std::mutex> lk(mutex_);
    release();
}

void BassStreamClip::release() {
    BassAudio::releaseSink(sink_);  // a late END event of the old stream must not call the new onEnd
    if (stream_ != 0) {
        BASS_StreamFree(stream_);
        stream_ = 0;
    }
}

}  // namespace bass
}  // namespace opsu
