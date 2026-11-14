// T2: short sound effect (hit sounds, UI sounds). Port of BassClip.java.
#include "BassClip.h"

#include <algorithm>
#include <exception>
#include <vector>

#include "../../third_party/bass/bass.h"

#include "BassBackend.h"
#include "opsu/logging/Log.h"

namespace opsu {
namespace bass {

namespace {

const char* const TAG = "BassClip";

/** Max simultaneous playbacks of a single sample; the furthest-along one is overridden beyond this. */
constexpr DWORD MAX_PLAYBACKS = 32;

}  // namespace

std::unique_ptr<BassClip> BassClip::load(const std::string& path) {
    try {
        std::vector<uint8_t> data;
        if (!BassAudio::readResource(path, data)) return std::unique_ptr<BassClip>(new BassClip(0));  // not found
        if (data.empty()) return std::unique_ptr<BassClip>(new BassClip(0));  // empty skin sound = intentional silence
        // The sample is decoded into BASS's own memory, so `data` may be freed after this call.
        const DWORD handle = BASS_SampleLoad(BASS_FILE_MEM, data.data(), 0, static_cast<DWORD>(data.size()),
                                             MAX_PLAYBACKS, BASS_SAMPLE_OVER_POS);
        if (handle == 0) {
            Log::warn(TAG, "BASS_SampleLoad failed for " + path + " (error " + std::to_string(BASS_ErrorGetCode()) + ")");
            return std::unique_ptr<BassClip>(new BassClip(0));
        }
        return std::unique_ptr<BassClip>(new BassClip(handle));
    } catch (const std::exception& e) {
        Log::warn(TAG, "Could not load " + path, e);
        return std::unique_ptr<BassClip>(new BassClip(0));
    }
}

BassClip::~BassClip() { destroy(); }

int BassClip::play(float volume, std::function<void()> /*onEnd: ignored, like Java*/) {
    const DWORD sample = sample_.load();
    if (sample == 0) return 0;
    const DWORD channel = BASS_SampleGetChannel(sample, BASS_SAMCHAN_NEW);
    if (channel == 0) return 0;
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, clampVolume(volume));
    BASS_ChannelPlay(channel, FALSE);
    return static_cast<int>(channel);
}

int BassClip::playLoop(float volume, float frequencyRatio) {
    const DWORD sample = sample_.load();
    if (sample == 0) return 0;
    const DWORD channel = BASS_SampleGetChannel(sample, BASS_SAMCHAN_NEW);
    if (channel == 0) return 0;
    BASS_ChannelFlags(channel, BASS_SAMPLE_LOOP, BASS_SAMPLE_LOOP);
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, clampVolume(volume));
    if (frequencyRatio != 1.f) setChannelFrequency(static_cast<int>(channel), frequencyRatio);
    BASS_ChannelPlay(channel, FALSE);
    return static_cast<int>(channel);
}

void BassClip::setChannelVolume(int channel, float volume) {
    BASS_ChannelSetAttribute(static_cast<DWORD>(channel), BASS_ATTRIB_VOL, clampVolume(volume));
}

void BassClip::setChannelFrequency(int channel, float frequencyRatio) {
    float base = 0.f;
    if (BASS_ChannelGetAttribute(static_cast<DWORD>(channel), BASS_ATTRIB_FREQ, &base) && base > 0.f) {
        // the attribute reports the current rate, so remember the sample's original rate the first time
        float orig;
        {
            std::lock_guard<std::mutex> lk(ratesMutex_);
            auto it = baseRates_.find(channel);
            if (it == baseRates_.end()) it = baseRates_.emplace(channel, base).first;
            orig = it->second;
        }
        const float f = std::max(100.f, std::min(100000.f, orig * frequencyRatio));
        BASS_ChannelSetAttribute(static_cast<DWORD>(channel), BASS_ATTRIB_FREQ, f);
    }
}

void BassClip::stopChannel(int channel) {
    BASS_ChannelStop(static_cast<DWORD>(channel));
    std::lock_guard<std::mutex> lk(ratesMutex_);
    baseRates_.erase(channel);
}

void BassClip::stop() {
    const DWORD sample = sample_.load();
    if (sample != 0) BASS_SampleStop(sample);
    // (not in Java) every channel of this sample just ended: forget their remembered rates so a recycled
    // channel handle can never inherit a stale base rate
    std::lock_guard<std::mutex> lk(ratesMutex_);
    baseRates_.clear();
}

void BassClip::destroy() {
    const DWORD sample = sample_.exchange(0);
    if (sample != 0) BASS_SampleFree(sample);
    std::lock_guard<std::mutex> lk(ratesMutex_);
    baseRates_.clear();
}

}  // namespace bass
}  // namespace opsu
