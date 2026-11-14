#include "opsu/audio/MusicController.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

#include "opsu/audio/IAudioBackend.h"
#include "opsu/audio/SilentAudioBackend.h"
#include "opsu/logging/Log.h"
#include "opsu/ui/animations/AnimationEquation.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"

namespace opsu {

namespace {

/** Java's (long) cast of a float/double: NaN -> 0, saturating. */
int64_t toLong(double v) {
    if (std::isnan(v)) return 0;
    if (v >= 9223372036854775807.0) return INT64_MAX;
    if (v <= -9223372036854775808.0) return INT64_MIN;
    return static_cast<int64_t>(v);
}

std::mutex g_soundStoreMutex;
std::weak_ptr<Music> g_soundStoreMusic;  // SoundStore.music: the last Music created

}  // namespace

// =====================================================================================================================
// Music
// =====================================================================================================================

std::shared_ptr<Music> Music::create(const std::string& path) {
    std::unique_ptr<AbsMusic> m;
    try {
        m = audioBackend().createMusic(path);
    } catch (const java::IOException& e) {
        Log::error("Music", "Could not load " + path, e);
        throw std::runtime_error("Could not load music: " + path);  // MusicController reports this to the user
    }
    return wrap(std::move(m));
}

std::shared_ptr<Music> Music::wrap(std::unique_ptr<AbsMusic> music) {
    std::shared_ptr<Music> m(new Music(std::move(music)));
    Music* raw = m.get();
    raw->music_->onComplete = [raw](AbsMusic&) { raw->complete(); };
    raw->music_->onRequestSync = [raw](AbsMusic&) {
        Log::debug("Music", "Music Request Sync");
        raw->syncPosition();
    };
    std::lock_guard<std::mutex> lock(g_soundStoreMutex);
    g_soundStoreMusic = m;
    return m;
}

Music::Music(std::unique_ptr<AbsMusic> music) : music_(std::move(music)) {
    lastTime_ = lastUpdateTime_ = audioMillis();  // assumed start time
}

Music::~Music() { dispose(); }

void Music::soundStoreSetMusicVolume(float musicVolume) {
    std::shared_ptr<Music> m;
    {
        std::lock_guard<std::mutex> lock(g_soundStoreMutex);
        m = g_soundStoreMusic.lock();
    }
    if (m) m->setMusicVolume(musicVolume);
}

void Music::soundStoreSetMusicPitch(float pitch) {
    std::shared_ptr<Music> m;
    {
        std::lock_guard<std::mutex> lock(g_soundStoreMutex);
        m = g_soundStoreMusic.lock();
    }
    if (m) m->setPitch(pitch);
}

// --- lead-in: positions before the start of the track --------------------------------------------------------------
// Backends can't seek to a negative position (SilentAudioBackend clamps it to 0), so a negative position is handled here:
// the backend is held at 0 while a virtual clock counts up to 0, then the backend is started at exactly the position the
// clock reached.

float Music::leadInCurrent() {
    if (!leadInRunning_) return leadInPos_;
    return leadInPos_ + static_cast<float>(audioMillis() - leadInStamp_) / 1000.f * pitch_;
}

void Music::leadInRebase() {
    leadInPos_ = leadInCurrent();
    leadInStamp_ = audioMillis();
}

void Music::leadInCancel() {
    leadInActive_ = false;
    leadInRunning_ = false;
}

bool Music::setPosition(float f) {
    if (f < 0.f) {
        bool wasPlaying = leadInActive_ ? leadInRunning_ : music_->playing();
        music_->setPosition(0.f);
        music_->pause();
        leadInActive_ = true;
        leadInRunning_ = wasPlaying;
        leadInPos_ = f;
        leadInStamp_ = audioMillis();
        syncPosition();
        return true;
    }
    // leaving the lead-in (e.g. skip): the backend was held paused at 0 meanwhile, so if the lead-in clock was running
    // the track must be started now, or the game would wait forever for audio
    bool resumeAfterSeek = leadInActive_ && leadInRunning_;
    leadInCancel();
    bool b = music_->setPosition(f);
    if (resumeAfterSeek) music_->resume();
    syncPosition();
    return b;
}

void Music::loop() {
    leadInCancel();
    music_->loop();
    syncPosition();
}

void Music::play() {
    leadInCancel();
    music_->play();
    syncPosition();
}

bool Music::playing() { return leadInActive_ ? leadInRunning_ : music_->playing(); }

void Music::pause() {
    if (leadInActive_) {
        leadInRebase();
        leadInRunning_ = false;
        return;
    }
    music_->pause();
}

void Music::resume() {
    if (leadInActive_) {
        if (!leadInRunning_) {
            leadInStamp_ = audioMillis();
            leadInRunning_ = true;
        }
        return;
    }
    music_->resume();
    syncPosition();
}

void Music::stop() {
    leadInCancel();
    music_->stop();
}

void Music::syncPosition() {
    lastPosition_ = music_->getPosition();
    lastTime_ = audioMillis() - toLong(lastPosition_ * 1000 / pitch_);
    avgDiff_ = 0;
    diffCnt_ = 0;
}

float Music::getPosition() {
    if (leadInActive_) {
        float p = leadInCurrent();
        if (p < 0.f || !leadInRunning_) return p;
        // lead-in is over: start the track at exactly where the virtual clock is
        leadInCancel();
        music_->resume();
        if (p > 0.f) music_->setPosition(p);
        syncPosition();
    }
    if (!music_->playing()) return music_->getPosition();
    float thisPosition = music_->getPosition();
    int64_t thisTime = audioMillis();
    float dxPosition2 = thisPosition - lastUpdatePosition_;

    float syncPos = thisPosition;
    float dxTime = static_cast<float>(thisTime - lastTime_) * pitch_;

    // Whenever the time changes check the difference between that and our current time; sync our time to song time
    if (std::fabs(syncPos - dxTime / 1000.f) > 0.2f) {
        Log::debug("Music", "Time HARD Reset " + std::to_string(syncPos) + " " + std::to_string(dxTime / 1000.f));
        syncPosition();
        dxTime = static_cast<float>(thisTime - lastTime_) * pitch_;
        avgDiff_ = 0;
    }
    float diff = syncPos * 1000 - dxTime;
    if (java::toInt(dxPosition2 * 1000) != 0) {
        if (diffCnt_ < 100) diffCnt_++;

        avgDiff_ = ((diff + (avgDiff_ * (diffCnt_ - 1))) / diffCnt_);
        float minDiff = avgDiff_ / 1;
        if (std::fabs(minDiff) >= 1) {
            lastTime_ -= java::toInt(minDiff);
            avgDiff_ -= java::toInt(minDiff);
        }
        dxTime = static_cast<float>(thisTime - lastTime_) * pitch_;
        lastUpdatePosition_ = thisPosition;
        lastUpdateTime_ = thisTime;
    }
    return dxTime / 1000.f;
}

void Music::dispose() {
    std::lock_guard<std::mutex> lock(disposeMutex_);
    if (disposed_) return;
    disposed_ = true;
    volumeFade_.active = false;
    pitchFade_.active = false;
    music_->onComplete = nullptr;
    music_->onRequestSync = nullptr;
    music_->dispose();
}

void Music::setMusicVolume(float musvolume) {
    music_->setVolume(musvolume);
    volume_ = musvolume;
}

void Music::complete() {
    // iterate over a copy: a listener may add listeners
    auto copy = listeners_;
    for (auto& l : copy) l(*this);
}

bool Music::getFFT(float* out256) { return music_ && music_->playing() && music_->getFFT(out256); }

bool Music::getLevels(float out[2]) { return music_ && music_->playing() && music_->getLevels(out); }

void Music::setLowPass(float cutoffHz) {
    if (music_ && !disposed_) music_->setLowPass(cutoffHz);
}

void Music::setPitch(float pitch) {
    if (leadInActive_) leadInRebase();  // rebase with the old pitch before it changes
    music_->setPitch(pitch);
    pitch_ = pitch;
    syncPosition();
}

void Music::pitchFade(int duration, float endPitch) {
    if (duration <= 0 || disposed_) return;  // (Java: a 0 duration would set the pitch to NaN)
    pitchFade_ = Fade{true, audioMillis(), pitch_, endPitch, duration, false};
}

void Music::fade(int duration, float endVolume, bool endAfterFade) {
    if (disposed_) return;
    volumeFade_ = Fade{true, audioMillis(), volume_, endVolume, std::max(duration, 0), endAfterFade};
}

void Music::updateFades() {
    int64_t now = audioMillis();
    if (pitchFade_.active) {
        Fade f = pitchFade_;
        int64_t dt = now - f.start;
        if (dt > f.duration) {
            pitchFade_.active = false;  // (Java: stops without applying this step)
        } else {
            float newPitch = f.from + ((f.to - f.from) * static_cast<float>(dt) / f.duration);
            setPitch(newPitch);
        }
    }
    if (volumeFade_.active) {
        Fade f = volumeFade_;
        int64_t dt = now - f.start;
        if (dt > f.duration) {
            setVolume(f.to);  // NOTE: Music::setVolume is empty in Java too (see class comment); use setMusicVolume to make fades audible
            volumeFade_.active = false;
            if (f.endAfterFade) pause();
        } else {
            float newVol = f.from + ((f.to - f.from) * static_cast<float>(dt) / f.duration);
            setVolume(newVol);
        }
    }
}

// =====================================================================================================================
// MusicController
// =====================================================================================================================

namespace {

constexpr float MUFFLED_CUTOFF = 300.f;   // cutoff (Hz) of the fully muffled track
constexpr int MUFFLE_IN_TIME = 500;       // ms, clear -> fully muffled
constexpr int MUFFLE_OUT_TIME = 700;      // ms, fully muffled -> clear
constexpr int VOLUME_ADJUST_HOLD = 1500;  // ms the track stays clear after the volume was last changed while muffled

std::shared_ptr<Music> player;            // accessed with std::atomic_load / atomic_store
const Beatmap* lastBeatmap = nullptr;     // the last beatmap passed to play()
int duration = 0;                         // the track duration
std::atomic<bool> trackEndedFlag{false};
bool themePlaying = false;
float pauseTime = 0.f;
bool trackDimmed = false;
float dimLevel = 1.f;
int timingPointIndex = 0;
std::optional<TimingPoint> lastNonInheritedTimingPoint;
std::optional<TimingPoint> lastTimingPoint;
float startVolumeScale = 1.f;
bool muffleRequested = false;
int volumeAdjustTimer = 0;
float muffleAmount = 0.f;
float appliedCutoff = MusicController::LOWPASS_OFF;

MusicController::Host& hostRef() {
    static MusicController::Host h = [] {
        MusicController::Host d;
        return d;
    }();
    return h;
}

void fillHostDefaults(MusicController::Host& h) {
    if (!h.musicVolume) h.musicVolume = [] { return 1.f; };
    if (!h.masterVolume) h.masterVolume = [] { return 1.f; };
    if (!h.musicOffset) h.musicOffset = [] { return 0; };
    if (!h.themeBeatmap) h.themeBeatmap = [] { return static_cast<const Beatmap*>(nullptr); };
    if (!h.trackExists) {
        h.trackExists = [](const std::string& path) {
            std::error_code ec;
            return std::filesystem::is_regular_file(path, ec);
        };
    }
    if (!h.barNotification) h.barNotification = [](const std::string& text) { Log::info("MusicController", text); };
    if (!h.error) {
        h.error = [](const std::string& message, const std::exception* e, bool) {
            if (e) Log::error("MusicController", message, *e);
            else Log::error("MusicController", message);
        };
    }
}

struct HostInit {
    HostInit() { fillHostDefaults(hostRef()); }
} hostInit;

std::string fileName(const std::string& path) {
    size_t i = path.find_last_of('/');
    return i == std::string::npos ? path : path.substr(i + 1);
}

/** BeatmapParser.getExtension(file): text after the last '.', lower case (ASCII); "" if none. */
std::string extensionOf(const std::string& file) {
    size_t i = file.find_last_of('.');
    if (i == std::string::npos) return "";
    std::string e = file.substr(i + 1);
    for (char& c : e) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return e;
}

void resetTimingPoint() {
    timingPointIndex = 0;
    lastTimingPoint.reset();
    lastNonInheritedTimingPoint.reset();
}

/** Sends the current cutoff to the player. `force`: even if it hasn't changed (for a freshly created player). */
void applyMuffle(bool force) {
    std::shared_ptr<Music> p = std::atomic_load(&player);
    if (!p) return;

    // ease the progress, then interpolate the cutoff in log-frequency so the sweep sounds even
    float cutoff = MusicController::LOWPASS_OFF;
    if (muffleAmount > 0.f) {
        float t = calc(AnimationEquation::IN_OUT_SINE, muffleAmount);
        double logCutoff = std::log(static_cast<double>(MusicController::LOWPASS_OFF)) +
                           (std::log(static_cast<double>(MUFFLED_CUTOFF)) - std::log(static_cast<double>(MusicController::LOWPASS_OFF))) * t;
        cutoff = std::min(MusicController::LOWPASS_OFF, static_cast<float>(std::exp(logCutoff)));
    }

    bool wasOff = appliedCutoff >= MusicController::LOWPASS_OFF, isOff = cutoff >= MusicController::LOWPASS_OFF;
    if (!force) {
        if (isOff && wasOff) return;  // still clear
        if (!isOff && !wasOff && std::fabs(std::log(static_cast<double>(cutoff / appliedCutoff))) < 0.005) return;  // too small a step to matter
    }
    appliedCutoff = cutoff;
    p->setLowPass(cutoff);
}

void loadTrack(const std::string& path, int position, bool loop) {
    try {
        // create the music player
        std::shared_ptr<Music> p = Music::create(path);
        std::atomic_store(&player, p);
        applyMuffle(true);  // (a track started while the loader is open is muffled from the first sample)
        p->addListener([](Music& music) {
            if (&music == std::atomic_load(&player).get()) {  // don't fire if music swapped
                trackEndedFlag = true;
                resetTimingPoint();
            }
        });
        MusicController::playAt(position, loop);
    } catch (const std::exception& e) {
        MusicController::host().error("Could not play track '" + fileName(path) + "'.", &e, false);
    }
}

/** Updates the timing point information for the current track position. false: not available. */
bool updateTimingPoint() {
    const Beatmap* map = MusicController::getBeatmap();
    if (!MusicController::isPlaying() || map == nullptr || !map->timingPoints.has_value() || map->timingPoints->empty()) return false;
    const std::vector<TimingPoint>& pts = *map->timingPoints;

    // initialization
    if (timingPointIndex == 0 && !lastNonInheritedTimingPoint.has_value() && !pts.empty()) {
        const TimingPoint& tp = pts[0];
        if (!tp.isInherited()) lastNonInheritedTimingPoint = tp;
        lastTimingPoint = tp;  // (Java: misleading indentation, this line is NOT part of the `if`)
    }

    // advance timing point index, record last non-inherited timing point
    int trackPosition = MusicController::getPosition(false);
    for (size_t i = static_cast<size_t>(timingPointIndex) + 1; i < pts.size(); i++) {
        const TimingPoint& tp = pts[i];
        if (trackPosition < tp.getTime()) break;
        timingPointIndex = static_cast<int>(i);
        if (!tp.isInherited() && tp.getBeatLength() > 0) lastNonInheritedTimingPoint = tp;
        lastTimingPoint = tp;
    }
    return lastNonInheritedTimingPoint.has_value();  // if empty, there is no timing info
}

void destroyOpenAL() {
    if (!MusicController::trackExists()) return;
    MusicController::stop();
    // Java: the OpenAL cleanup is commented out; only `player = null`. Dropping the last reference disposes the backend track.
    std::atomic_store(&player, std::shared_ptr<Music>());
}

}  // namespace

void MusicController::setHost(Host h) {
    fillHostDefaults(h);
    hostRef() = std::move(h);
}

const MusicController::Host& MusicController::host() { return hostRef(); }

std::shared_ptr<Music> MusicController::getPlayer() { return std::atomic_load(&player); }

void MusicController::setMuffled(bool muffled) {
    muffleRequested = muffled;
    if (!muffled) volumeAdjustTimer = 0;
}

void MusicController::onVolumeAdjusted() {
    if (muffleRequested) volumeAdjustTimer = VOLUME_ADJUST_HOLD;
}

void MusicController::updateMuffle(int delta) {
    delta = std::max(0, std::min(delta, 50));  // (a long frame mustn't skip the animation)
    if (std::shared_ptr<Music> p = std::atomic_load(&player)) p->updateFades();
    if (volumeAdjustTimer > 0) volumeAdjustTimer = std::max(0, volumeAdjustTimer - delta);

    bool muffle = muffleRequested && volumeAdjustTimer == 0;
    if (muffle && muffleAmount < 1.f) muffleAmount = std::min(1.f, muffleAmount + delta / static_cast<float>(MUFFLE_IN_TIME));
    else if (!muffle && muffleAmount > 0.f) muffleAmount = std::max(0.f, muffleAmount - delta / static_cast<float>(MUFFLE_OUT_TIME));

    applyMuffle(false);
}

void MusicController::setStartVolumeScale(float scale) { startVolumeScale = std::max(0.f, std::min(1.f, scale)); }

void MusicController::play(const Beatmap& beatmap, bool loop, bool preview) {
    // new track: load and play
    if (lastBeatmap == nullptr || beatmap.audioFilename != lastBeatmap->audioFilename) {
        const std::string& audioFile = beatmap.audioFilename;
        if (!host().trackExists(audioFile)) {
            host().barNotification("Could not find track '" + fileName(audioFile) + "'.");
            return;
        }

        reset();

        std::string ext = extensionOf(fileName(beatmap.audioFilename));
        if (ext == "ogg" || ext == "mp3") loadTrack(audioFile, preview ? beatmap.previewTime : 0, loop);
    }

    // new track position: play at position
    else if (beatmap.previewTime != lastBeatmap->previewTime)
        playAt(beatmap.previewTime, loop);

    lastBeatmap = &beatmap;
}

void MusicController::playAt(int position, bool loop) {
    if (trackExists()) {
        setVolume(host().musicVolume() * host().masterVolume() * startVolumeScale);
        trackEndedFlag = false;
        pauseTime = 0.f;
        resetTimingPoint();
        std::shared_ptr<Music> p = getPlayer();
        if (loop) p->loop();
        else p->play();
        if (position >= 0) p->setPosition(position / 1000.f);
    }
}

bool MusicController::trackExists() { return std::atomic_load(&player) != nullptr; }

bool MusicController::getLevels(float out[2]) {
    std::shared_ptr<Music> p = getPlayer();
    return p && p->getLevels(out);
}

bool MusicController::getFFT(float* out256) {
    std::shared_ptr<Music> p = getPlayer();
    return p && p->getFFT(out256);
}

const Beatmap* MusicController::getBeatmap() { return lastBeatmap; }

float MusicController::getBeatProgress() {
    if (!updateTimingPoint()) return static_cast<float>(audioMillis() % 1000) / 1000.f;
    // calculate beat progress
    int trackPosition = std::max(0, getPosition(false));
    const TimingPoint& tp = *lastNonInheritedTimingPoint;
    double beatLength = tp.getBeatLength() * 100.0;
    int beatTime = tp.getTime();
    if (trackPosition < beatTime) trackPosition = java::toInt(trackPosition + (beatLength / 100.0) * (beatTime / tp.getBeatLength()));
    return static_cast<float>(std::fmod((trackPosition - beatTime) * 100.0, beatLength) / beatLength);
}

TimingPoint MusicController::getLastNonInheritedTimingPoint() {
    if (!updateTimingPoint()) return TimingPoint::DEFAULT;  // kww: null -> TimingPoint.DEFAULT
    return *lastNonInheritedTimingPoint;
}

TimingPoint MusicController::getLastTimingPoint() {
    if (!updateTimingPoint()) return TimingPoint::DEFAULT;  // kww: null -> TimingPoint.DEFAULT
    return *lastTimingPoint;
}

std::optional<float> MusicController::getMeasureProgress(int k) {
    if (!updateTimingPoint()) return std::nullopt;
    // calculate measure progress
    int trackPosition = std::max(0, getPosition(false));
    const TimingPoint& tp = *lastNonInheritedTimingPoint;
    double measureLength = tp.getBeatLength() * tp.getMeter() * k * 100.0;
    int beatTime = tp.getTime();
    if (trackPosition < beatTime) trackPosition = java::toInt(trackPosition + (measureLength / 100.0) * (beatTime / tp.getBeatLength()));
    return static_cast<float>(std::fmod((trackPosition - beatTime) * 100.0, measureLength) / measureLength);
}

bool MusicController::isPlaying() {
    std::shared_ptr<Music> p = getPlayer();
    return p && p->playing();
}

bool MusicController::isPaused() { return trackExists() && pauseTime > 0.f; }

void MusicController::pause() {
    if (isPlaying()) {
        pauseTime = getPlayer()->getPosition();
        getPlayer()->pause();
    }
}

void MusicController::resume() {
    if (trackExists()) {
        pauseTime = 0.f;
        getPlayer()->resume();
        getPlayer()->setVolume(1.0f);
    }
}

void MusicController::stop() {
    if (isPlaying()) getPlayer()->stop();
    if (trackExists()) {
        pauseTime = 0.f;
        resetTimingPoint();
    }
}

void MusicController::fadeOut(int d) {
    if (isPlaying()) getPlayer()->fade(d, 0.f, true);
}

void MusicController::pitchFadeOut(int d) {
    if (isPlaying()) getPlayer()->pitchFade(d, 0.f);
}

int MusicController::getPosition(bool useOffsets) {
    std::shared_ptr<Music> p = getPlayer();
    if (!p) return 0;
    int offset = useOffsets ? getTotalOffset() : 0;
    return java::toInt(p->getPosition() * 1000 + offset);
}

int MusicController::getTotalOffset() {
    int offset = host().musicOffset();
    if (lastBeatmap != nullptr) offset += lastBeatmap->localMusicOffset;
    return offset;
}

bool MusicController::setTrackPosition(int trackPosition) { return setPosition(trackPosition - getTotalOffset()); }

bool MusicController::setPosition(int position) {
    if (!trackExists()) return false;
    resetTimingPoint();
    return getPlayer()->setPosition(position / 1000.f);
}

int MusicController::getDuration() {
    if (!trackExists() || lastBeatmap == nullptr) return -1;
    if (duration == 0) {
        // fallback: use beatmap end time (often not the track duration)
        duration = lastBeatmap->endTime;
    }
    return duration;
}

void MusicController::play(bool loop) {
    if (trackExists()) {
        trackEndedFlag = false;
        resetTimingPoint();
        if (loop) getPlayer()->loop();
        else getPlayer()->play();
    }
}

void MusicController::setVolume(float volume) { Music::soundStoreSetMusicVolume(isTrackDimmed() ? volume * dimLevel : volume); }

void MusicController::setPitch(float pitch) {
    if (trackExists()) getPlayer()->setPitch(pitch);
}

bool MusicController::trackEnded() { return trackEndedFlag; }

void MusicController::loopTrackIfEnded(bool preview) {
    if (trackEndedFlag && trackExists()) playAt(preview ? lastBeatmap->previewTime : 0, false);
}

void MusicController::playThemeSong() {
    const Beatmap* beatmap = host().themeBeatmap();
    if (beatmap != nullptr) {
        play(*beatmap, false, false);
        themePlaying = true;
    }
}

bool MusicController::isThemePlaying() { return themePlaying; }

bool MusicController::isTrackDimmed() { return trackDimmed; }

void MusicController::toggleTrackDimmed(float multiplier) {
    float volume = host().musicVolume() * host().masterVolume();
    dimLevel = isTrackDimmed() ? 1.f : multiplier;
    trackDimmed = !trackDimmed;
    setVolume(volume);
}

void MusicController::reset() {
    stop();

    // reset state
    lastBeatmap = nullptr;
    duration = 0;
    trackEndedFlag = false;
    themePlaying = false;
    pauseTime = 0.f;
    trackDimmed = false;
    resetTimingPoint();

    // releases the previous track
    destroyOpenAL();
}

}  // namespace opsu
