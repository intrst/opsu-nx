// T2: streaming music track backed by BASS. Port of BassMusic.java.
#include "BassMusic.h"

#include <algorithm>

#include "../../third_party/bass/bass.h"
#include "../../third_party/bass/bass_fx.h"

#include "opsu/logging/Log.h"
#include "opsu/util/JavaIO.h"

namespace opsu {
namespace bass {

namespace {

const char* const TAG = "BassMusic";

/**
 * true: speed mods keep the original pitch (like osu! stable DT/HT).
 * false: speed and pitch change together (the previous opsu behaviour).
 */
constexpr bool PRESERVE_PITCH = true;

/**
 * Subtract the output device latency from the reported position while playing, so the position is what the
 * player is hearing, not what has merely been sent to the device. NOTE: BassAudio::getOutputLatencyMs()
 * returns 0 unless BassAudio::setPollOutputLatency(true) was called (see docs/agents/T2.md).
 */
constexpr bool COMPENSATE_DEVICE_LATENCY = true;

/** Lowest rate accepted; the fail-fade drives the pitch towards 0, which BASS_FX cannot represent. */
constexpr float MIN_RATE = 0.05f;

/** MusicController::LOWPASS_OFF (T1): a cutoff at or above this removes the muffle effect. */
constexpr float LOWPASS_OFF = 20000.f;
/** Butterworth Q of the muffle biquad. */
constexpr float LOWPASS_Q = 0.7071f;

}  // namespace

BassMusic::BassMusic(const std::string& path) : path_(path) {
    const DWORD flags = BASS_STREAM_PRESCAN | BASS_STREAM_DECODE;

    DWORD source = 0;
    if (BassAudio::isPlainFile(path)) source = BASS_StreamCreateFile(BASS_FILE_NAME, path.c_str(), 0, 0, flags);
    if (source == 0) {
        // Asset, or a path BASS could not read: load into memory.
        if (!BassAudio::readResource(path, memory_)) throw java::IOException("File not found: " + path);
        if (memory_.empty()) throw java::IOException("BASS_StreamCreateFile failed for " + path + " (empty file)");
        source = BASS_StreamCreateFile(BASS_FILE_MEM, memory_.data(), 0, memory_.size(), flags);
    }
    if (source == 0)
        throw java::IOException("BASS_StreamCreateFile failed for " + path + " (error " +
                                std::to_string(BASS_ErrorGetCode()) + ")");

    // Decode stream -> tempo stream, so DT/HT can change speed without changing pitch.
    const DWORD t = BASS_FX_TempoCreate(source, BASS_FX_FREESOURCE);
    if (t == 0) {
        const int err = BASS_ErrorGetCode();
        BASS_StreamFree(source);
        throw java::IOException("BASS_FX_TempoCreate failed for " + path + " (error " + std::to_string(err) + ")");
    }
    stream_.store(t);

    BASS_CHANNELINFO info;
    if (!BASS_ChannelGetInfo(t, &info)) info.freq = 0;
    baseFreq_ = info.freq > 0 ? static_cast<float>(info.freq) : 44100.f;

    // Fires on BASS's thread -> BassAudio queues it for the game thread (pumpEvents). `fired` is Java's `ended`.
    endSink_ = BassAudio::attachEndSync(t, [this]() { fireMusicEnded(); });
    if (!endSink_) Log::warn(TAG, "BASS_ChannelSetSync(END) failed for " + path);
    Log::info(TAG, "Loaded " + path);
}

BassMusic::~BassMusic() { dispose(); }

void BassMusic::restartIfEnded() {
    if (endSink_ && endSink_->fired.load(std::memory_order_acquire)) {
        BASS_ChannelSetPosition(stream_.load(), 0, BASS_POS_BYTE);
        endSink_->fired.store(false);
    }
}

void BassMusic::setLoop(bool loop) {
    BASS_ChannelFlags(stream_.load(), loop ? BASS_SAMPLE_LOOP : 0, BASS_SAMPLE_LOOP);
}

bool BassMusic::setPosition(float seconds) {
    const DWORD s = stream_.load();
    if (s == 0) return false;
    const QWORD bytes = BASS_ChannelSeconds2Bytes(s, std::max(0.f, seconds));
    const bool ok = BASS_ChannelSetPosition(s, bytes, BASS_POS_BYTE) != 0;
    if (ok && endSink_) endSink_->fired.store(false);
    return ok;
}

void BassMusic::loop() {
    const DWORD s = stream_.load();
    if (s == 0) return;
    setLoop(true);
    restartIfEnded();
    BASS_ChannelPlay(s, FALSE);
}

void BassMusic::play() {
    const DWORD s = stream_.load();
    if (s == 0) return;
    setLoop(false);
    restartIfEnded();
    BASS_ChannelPlay(s, FALSE);
}

bool BassMusic::playing() {
    const DWORD s = stream_.load();
    if (s == 0) return false;
    const DWORD active = BASS_ChannelIsActive(s);
    return active == BASS_ACTIVE_PLAYING || active == BASS_ACTIVE_STALLED;
}

void BassMusic::pause() {
    const DWORD s = stream_.load();
    if (s != 0) BASS_ChannelPause(s);
}

void BassMusic::resume() {
    const DWORD s = stream_.load();
    if (s != 0) BASS_ChannelPlay(s, FALSE);
}

void BassMusic::stop() {
    // Matches the old players: halt output but keep the position.
    const DWORD s = stream_.load();
    if (s != 0) BASS_ChannelPause(s);
}

void BassMusic::setVolume(float volume) {
    const DWORD s = stream_.load();
    if (s != 0) BASS_ChannelSetAttribute(s, BASS_ATTRIB_VOL, clampVolume(volume));
}

float BassMusic::getPosition() {
    const DWORD s = stream_.load();
    if (s == 0) return 0.f;
    const QWORD bytes = BASS_ChannelGetPosition(s, BASS_POS_BYTE);
    if (static_cast<int64_t>(bytes) < 0) return 0.f;  // (QWORD)-1 = error
    float pos = static_cast<float>(BASS_ChannelBytes2Seconds(s, bytes));
    // Only while playing: when paused/seeking the raw position is what the UI expects.
    if (COMPENSATE_DEVICE_LATENCY && BASS_ChannelIsActive(s) == BASS_ACTIVE_PLAYING)
        pos -= BassAudio::getOutputLatencyMs() / 1000.f * rate_.load();
    return pos;
}

void BassMusic::dispose() {
    const DWORD s = stream_.exchange(0);
    if (s == 0) return;
    BassAudio::releaseSink(endSink_);  // late BASS-thread events can no longer reach this object
    BASS_ChannelStop(s);
    BASS_StreamFree(s);  // also frees the source stream (BASS_FX_FREESOURCE) and the muffle effect
    lowPassFx_ = 0;
    lowPassHz_ = 0.f;
    std::vector<uint8_t>().swap(memory_);
}

void BassMusic::setPitch(float pitch) {
    const DWORD s = stream_.load();
    if (s == 0) return;
    const float r = std::max(MIN_RATE, pitch);
    rate_.store(r);
    if (PRESERVE_PITCH) {
        // percent change: 1.5x -> +50, 0.75x -> -25
        BASS_ChannelSetAttribute(s, BASS_ATTRIB_TEMPO, (r - 1.f) * 100.f);
    } else {
        BASS_ChannelSetAttribute(s, BASS_ATTRIB_TEMPO, 0.f);
        BASS_ChannelSetAttribute(s, BASS_ATTRIB_TEMPO_FREQ, baseFreq_ * r);
    }
}

/** Same as osu-framework's TrackBass: peak of the last 1/60 s, per channel. */
bool BassMusic::getLevels(float out[2]) {
    const DWORD s = stream_.load();
    if (s == 0 || out == nullptr) return false;
    float tmp[2];
    if (!BASS_ChannelGetLevelEx(s, tmp, 1.f / 60.f, BASS_LEVEL_STEREO)) return false;
    out[0] = tmp[0];
    out[1] = tmp[1];
    return true;
}

/** Same as osu-framework's TrackBass: BASS_DATA_FFT512 -> 256 magnitudes. */
bool BassMusic::getFFT(float* out256) {
    const DWORD s = stream_.load();
    if (s == 0 || out256 == nullptr) return false;
    float tmp[256];
    const DWORD r = BASS_ChannelGetData(s, tmp, BASS_DATA_FFT512);
    if (r == static_cast<DWORD>(-1)) return false;
    std::copy(tmp, tmp + 256, out256);
    return true;
}

/**
 * Muffle. NOT in the original BassMusic.java (AbsMusic's default was a no-op); implemented from AGENT_TASKS.md:
 * a BASS_FX biquad low-pass, Q = 0.7071, all channels. >= 20000 Hz removes the effect; otherwise the effect is
 * created once and later calls only update its parameters (this is called every frame).
 */
void BassMusic::setLowPass(float cutoffHz) {
    const DWORD s = stream_.load();
    if (s == 0) return;

    // `!(x < OFF)` also catches NaN.
    if (!(cutoffHz < LOWPASS_OFF)) {
        if (lowPassFx_ != 0) {
            BASS_ChannelRemoveFX(s, lowPassFx_);
            lowPassFx_ = 0;
            lowPassHz_ = 0.f;
        }
        return;
    }

    // BASS_FX wants 1 Hz .. < freq/2.
    const float nyquist = baseFreq_ * 0.5f;
    const float hz = std::max(1.f, std::min(cutoffHz, nyquist - 1.f));

    if (lowPassFx_ != 0 && hz == lowPassHz_) return;  // nothing changed: no BASS call

    BASS_BFX_BQF p;
    p.lFilter = BASS_BFX_BQF_LOWPASS;
    p.fCenter = hz;
    p.fGain = 0.f;       // only used by peaking/shelf filters
    p.fBandwidth = 0.f;  // fBandwidth has priority over fQ, so 0 = "not in use" -> fQ below is the Q
    p.fQ = LOWPASS_Q;
    p.fS = 1.f;          // only used by shelf filters
    p.lChannel = BASS_BFX_CHANALL;

    if (lowPassFx_ == 0) {
        lowPassFx_ = BASS_ChannelSetFX(s, BASS_FX_BFX_BQF, 0);
        if (lowPassFx_ == 0) {
            if (!lowPassWarned_) {
                lowPassWarned_ = true;
                Log::warn(TAG, "muffle: BASS_ChannelSetFX failed (error " + std::to_string(BASS_ErrorGetCode()) + ")");
            }
            return;
        }
    }
    if (BASS_FXSetParameters(lowPassFx_, &p))
        lowPassHz_ = hz;
    else
        lowPassHz_ = 0.f;  // parameters not applied: try again on the next call
}

}  // namespace bass
}  // namespace opsu
