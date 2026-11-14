#include "opsu/audio/SoundController.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <mutex>
#include <regex>
#include <unordered_set>

#include "opsu/logging/Log.h"

namespace opsu {

namespace {

namespace fs = std::filesystem;
using ClipPtr = std::shared_ptr<MultiClip>;

SoundController::Host& hostRef() {
    static SoundController::Host h;
    return h;
}

void fillHostDefaults(SoundController::Host& h) {
    if (!h.effectVolume) h.effectVolume = [] { return 1.f; };
    if (!h.hitSoundVolume) h.hitSoundVolume = [] { return 1.f; };
    if (!h.masterVolume) h.masterVolume = [] { return 1.f; };
    if (!h.musicVolume) h.musicVolume = [] { return 1.f; };
    if (!h.soundDisabled) h.soundDisabled = [] { return false; };
    if (!h.gameplaySoundDisabled) h.gameplaySoundDisabled = [] { return false; };
    if (!h.skinDirectory) h.skinDirectory = [] { return std::string(); };
    if (!h.layeredHitSounds) h.layeredHitSounds = [] { return true; };
    if (!h.resourceExists) h.resourceExists = [](const std::string&) { return false; };
    if (!h.isFile) h.isFile = [](const std::string& p) { std::error_code ec; return fs::is_regular_file(p, ec); };
    if (!h.isDirectory) h.isDirectory = [](const std::string& p) { std::error_code ec; return fs::is_directory(p, ec); };
    if (!h.listFiles) {
        h.listFiles = [](const std::string& dir) {
            std::vector<std::string> names;
            std::error_code ec;
            for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) names.push_back(it->path().filename().string());
            return names;
        };
    }
    if (!h.redNotification) h.redNotification = [](const std::string& t) { Log::warn("SoundController", t); };
    if (!h.barNotification) h.barNotification = [](const std::string& t) { Log::info("SoundController", t); };
    if (!h.error) {
        h.error = [](const std::string& message, const std::exception* e, bool) {
            if (e) Log::error("SoundController", message, *e);
            else Log::error("SoundController", message);
        };
    }
}

struct HostInit {
    HostInit() { fillHostDefaults(hostRef()); }
} hostInit;

const SoundController::Host& H() { return hostRef(); }

// --- state ---------------------------------------------------------------------------------------------------------

std::array<ClipPtr, SoundEffect_SIZE> effectClips;
std::array<std::array<ClipPtr, SampleSet_SIZE>, HitSound_SIZE> hitClips;

std::mutex trackMutex;
ClipPtr currentTrack;           // the current track being played, if any
float sampleVolumeMultiplier = 1.f;
bool isMuted = false;
std::optional<std::string> currentFileName;
int currentFileIndex = -1;

constexpr float MINIMUM_SAMPLE_VOLUME = 0.05f;  // osu!lazer never plays hit samples quieter than this
constexpr int LEGACY_NORMAL = 1;                // legacy "normal" hit sound bit (not part of HitObject's constants)
int timingPointCustomIndex = 0;
std::map<std::string, ClipPtr> beatmapClips;    // hit samples from the beatmap's folder, lower-case name without extension

std::mutex loopsMutex;
std::unordered_set<SoundController::Loop*> activeLoops;

ClipPtr introHit, introFinish;

size_t idx(SoundEffect e) { return static_cast<size_t>(e); }
size_t idx(HitSound s) { return static_cast<size_t>(s); }
size_t idx(SampleSet s) { return static_cast<size_t>(s); }

ClipPtr loadClip(const std::string& ref) { return std::make_shared<MultiClip>(ref); }  // (Java: never null, a failed load is a silent clip)

std::string asciiLower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::string stripExtension(const std::string& name) {
    size_t dot = name.find_last_of('.');
    return (dot != std::string::npos && dot > 0) ? name.substr(0, dot) : name;
}

std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    return dir.back() == '/' ? dir + name : dir + "/" + name;
}

/** File.getAbsolutePath(): no normalisation. */
std::string absolutePath(const std::string& p) {
    std::error_code ec;
    fs::path a = fs::absolute(fs::path(p), ec);
    return ec ? p : a.string();
}

/** The sound file name, with extension, looking through the skin directory first and then the default resources; nullopt if none. */
std::optional<std::string> getSoundFileName(const std::string& filename) {
    std::string wav = filename + ".wav", mp3 = filename + ".mp3", ogg = filename + ".ogg";
    std::string skinDir = H().skinDirectory();
    if (!skinDir.empty()) {
        std::string skinWAV = joinPath(skinDir, wav), skinMP3 = joinPath(skinDir, mp3), skinOGG = joinPath(skinDir, ogg);
        if (H().isFile(skinWAV)) return absolutePath(skinWAV);
        if (H().isFile(skinMP3)) return absolutePath(skinMP3);
        if (H().isFile(skinOGG)) return absolutePath(skinOGG);
    }
    if (H().resourceExists(wav)) return wav;
    if (H().resourceExists(mp3)) return mp3;
    if (H().resourceExists(ogg)) return ogg;
    return std::nullopt;
}

void playClip(const ClipPtr& clip, float volume, std::function<void()> listener) {
    if (!clip) return;  // clip failed to load properly
    if (volume > 0.f && !isMuted) clip->start(volume, std::move(listener));
}

float getObjectVolume(const HitObject& hitObject) {
    int objectVolume = hitObject.getHitSoundVolume();
    float volume = (objectVolume > 0) ? std::min(objectVolume, 100) / 100.f : sampleVolumeMultiplier;
    return std::max(volume, MINIMUM_SAMPLE_VOLUME);
}

float getOutputVolume(float sampleVolume) { return H().hitSoundVolume() * sampleVolume * H().masterVolume(); }

int getCustomIndex(const HitObject& hitObject) {
    int index = hitObject.getCustomSampleIndex();
    return (index != 0) ? index : timingPointCustomIndex;
}

/** The clip for a hit sound, in the order of osu!lazer: beatmap samples with the custom suffix, beatmap samples without it, then skin / default. */
ClipPtr findClip(SampleSet bank, HitSound sound, int customIndex) {
    if (customIndex >= 1) {
        std::string name = std::string(getName(bank)) + "-" + getFileName(sound);
        ClipPtr clip;
        if (customIndex >= 2) {
            auto it = beatmapClips.find(name + std::to_string(customIndex));
            if (it != beatmapClips.end()) clip = it->second;
        }
        if (!clip) {
            auto it = beatmapClips.find(name);
            if (it != beatmapClips.end()) clip = it->second;
        }
        if (clip) return clip;
    }
    return hitClips[idx(sound)][idx(bank)];
}

/** Whether the 'normal' sample of this hit sound type is a layered one (no normal bit set). */
bool isLayered(int16_t hitSoundType) { return hitSoundType != 0 && (hitSoundType & LEGACY_NORMAL) == 0; }

}  // namespace

// =====================================================================================================================

void SoundController::setHost(Host h) {
    fillHostDefaults(h);
    hostRef() = std::move(h);
}

const SoundController::Host& SoundController::host() { return hostRef(); }

ClipPtr SoundController::getClip(SoundEffect e) { return effectClips[idx(e)]; }
void SoundController::setClip(SoundEffect e, ClipPtr clip) { effectClips[idx(e)] = std::move(clip); }
ClipPtr SoundController::getClip(HitSound s, SampleSet set) { return hitClips[idx(s)][idx(set)]; }
void SoundController::setClip(HitSound s, SampleSet set, ClipPtr clip) { hitClips[idx(s)][idx(set)] = std::move(clip); }
ClipPtr SoundController::getClip(HitSound s) {
    std::optional<SampleSet> active = hitsound::activeSampleSet();
    return active ? hitClips[idx(s)][idx(*active)] : nullptr;
}

void SoundController::init() {
    if (H().soundDisabled()) return;

    currentFileIndex = 0;
    int failedCount = 0;

    // menu and game sounds
    for (SoundEffect s : soundEffectValues()) {
        currentFileName = getSoundFileName(getFileName(s));
        if (!currentFileName) {
            H().error(std::string("Could not find sound file '") + getFileName(s) + "'.", nullptr, false);
            continue;
        }
        ClipPtr newClip = loadClip(*currentFileName);
        if (!newClip) failedCount++;
        if (getClip(s)) {  // clip previously loaded (e.g. program restart)
            if (newClip) {
                getClip(s)->destroy();  // destroy previous clip
                setClip(s, newClip);
            }
        } else
            setClip(s, newClip);
        currentFileIndex++;
    }

    // hit sounds
    for (SampleSet ss : sampleSetValues()) {
        for (HitSound s : hitSoundValues()) {
            std::string filename = std::string(getName(ss)) + "-" + getFileName(s);
            currentFileName = getSoundFileName(filename);
            if (!currentFileName) {
                H().error("Could not find hit sound file '" + filename + "'.", nullptr, false);
                continue;
            }
            ClipPtr newClip = loadClip(*currentFileName);
            if (!newClip) failedCount++;
            if (getClip(s, ss)) {  // clip previously loaded (e.g. program restart)
                if (newClip) {
                    getClip(s, ss)->destroy();  // destroy previous clip
                    setClip(s, ss, newClip);
                }
            } else
                setClip(s, ss, newClip);
            currentFileIndex++;
        }
    }

    currentFileName.reset();
    currentFileIndex = -1;

    // show a notification if any files failed to load
    if (failedCount > 0)
        H().redNotification("Failed to load " + std::to_string(failedCount) + " audio file" + (failedCount == 1 ? "" : "s") + ".");
}

void SoundController::setSampleVolume(float volume) {
    if (volume >= 0.f && volume <= 1.f) sampleVolumeMultiplier = volume;
}

void SoundController::playSound(SoundEffect s) { playClip(getClip(s), H().effectVolume() * H().masterVolume(), nullptr); }
void SoundController::playSound(HitSound s) { playClip(getClip(s), H().effectVolume() * H().masterVolume(), nullptr); }

bool SoundController::playSoundChecked(SoundEffect s) {
    ClipPtr clip = getClip(s);
    float volume = H().effectVolume() * H().masterVolume();
    if (!clip || volume <= 0.f || isMuted) return true;
    clip->start(volume, nullptr);
    return clip->getLastPlayId() != 0;
}

bool SoundController::playSoundChecked(HitSound s) {
    ClipPtr clip = getClip(s);
    float volume = H().effectVolume() * H().masterVolume();
    if (!clip || volume <= 0.f || isMuted) return true;
    clip->start(volume, nullptr);
    return clip->getLastPlayId() != 0;
}

void SoundController::stopSound(SoundEffect s) {
    if (ClipPtr c = getClip(s)) c->stop();
}

void SoundController::stopSound(HitSound s) {
    if (ClipPtr c = getClip(s)) c->stop();
}

void SoundController::setTimingPointCustomIndex(int index) { timingPointCustomIndex = index; }

void SoundController::playHitSound(const HitObject& hitObject, int edgeIndex) {
    int16_t type = hitObject.getEdgeHitSoundType(edgeIndex);
    if (type < 0) return;

    float volume = getOutputVolume(getObjectVolume(hitObject));
    if (volume <= 0.f) return;

    SampleSet normalBank = hitsound::resolveSampleSet(hitObject.getSampleSet(edgeIndex));
    int8_t additionIndex = hitObject.getAdditionSampleSet(edgeIndex);
    SampleSet additionBank = (additionIndex != 0) ? hitsound::resolveSampleSet(additionIndex) : normalBank;
    int custom = getCustomIndex(hitObject);

    // normal sample: replaced by the beatmap's own file if the object names one;
    // it is a layered sample (skippable via skin.ini) if the type has no normal bit
    ClipPtr normalClip;
    const std::string& file = hitObject.getHitSoundFile();
    if (!file.empty()) {
        auto it = beatmapClips.find(asciiLower(stripExtension(file)));
        if (it != beatmapClips.end()) normalClip = it->second;
    }
    if (normalClip)
        playClip(normalClip, volume, nullptr);
    else if (!isLayered(type) || H().layeredHitSounds())
        playClip(findClip(normalBank, HitSound::NORMAL, custom), volume, nullptr);

    // additions
    if ((type & HitObject::SOUND_WHISTLE) != 0) playClip(findClip(additionBank, HitSound::WHISTLE, custom), volume, nullptr);
    if ((type & HitObject::SOUND_FINISH) != 0) playClip(findClip(additionBank, HitSound::FINISH, custom), volume, nullptr);
    if ((type & HitObject::SOUND_CLAP) != 0) playClip(findClip(additionBank, HitSound::CLAP, custom), volume, nullptr);
}

void SoundController::playSliderTick(const HitObject& hitObject) {
    if (isLayered(hitObject.getHitSoundType()) && !H().layeredHitSounds()) return;
    float volume = getOutputVolume(getObjectVolume(hitObject));
    SampleSet bank = hitsound::resolveSampleSet(hitObject.getObjectSampleSet());
    playClip(findClip(bank, HitSound::SLIDERTICK, getCustomIndex(hitObject)), volume, nullptr);
}

void SoundController::playHitSound(HitSound s) {
    playClip(getClip(s), H().hitSoundVolume() * sampleVolumeMultiplier * H().masterVolume(), nullptr);
}

void SoundController::preloadIntroSounds() {
    if (H().soundDisabled() || introHit) return;
    try {
        std::optional<std::string> name = getSoundFileName("normal-hitnormal");
        if (name) introHit = loadClip(*name);
        name = getSoundFileName("normal-hitfinish");
        if (name) introFinish = loadClip(*name);
    } catch (...) {
        introHit.reset();
        introFinish.reset();
    }
}

void SoundController::playIntroHit(bool finish) {
    float volume = H().hitSoundVolume() * sampleVolumeMultiplier * H().masterVolume();
    playClip(introHit, volume, nullptr);
    if (finish) playClip(introFinish, volume, nullptr);
}

// --- loops ---------------------------------------------------------------------------------------------------------

SoundController::Loop::~Loop() { stop(); }

void SoundController::Loop::start(ClipPtr clip, float volume, float frequencyRatio) {
    if (id_ != 0 || !clip || isMuted || volume <= 0.f) return;
    id_ = clip->startLoop(volume, frequencyRatio);
    if (id_ != 0) {
        clip_ = std::move(clip);
        std::lock_guard<std::mutex> lock(loopsMutex);
        activeLoops.insert(this);
    }
}

void SoundController::Loop::setVolume(float volume) {
    if (id_ != 0) clip_->setLoopVolume(id_, volume);
}

void SoundController::Loop::setFrequency(float ratio) {
    if (id_ != 0) clip_->setLoopFrequency(id_, ratio);
}

void SoundController::Loop::stop() {
    if (id_ == 0) return;
    clip_->stopLoop(id_);
    id_ = 0;
    clip_.reset();
    std::lock_guard<std::mutex> lock(loopsMutex);
    activeLoops.erase(this);
}

void SoundController::SlidingSound::start() {
    if (slide_.isPlaying() || whistle_.isPlaying() || isMuted || H().gameplaySoundDisabled()) return;
    const HitObject& ho = *hitObject_;
    int16_t type = ho.getHitSoundType();
    float volume = getOutputVolume(getObjectVolume(ho));
    int custom = getCustomIndex(ho);
    SampleSet normalBank = hitsound::resolveSampleSet(ho.getObjectSampleSet());
    int8_t additionIndex = ho.getObjectAdditionSampleSet();
    SampleSet additionBank = (additionIndex != 0) ? hitsound::resolveSampleSet(additionIndex) : normalBank;

    if (!isLayered(type) || H().layeredHitSounds()) slide_.start(findClip(normalBank, HitSound::SLIDERSLIDE, custom), volume, 1.f);
    if ((type & HitObject::SOUND_WHISTLE) != 0) whistle_.start(findClip(additionBank, HitSound::SLIDERWHISTLE, custom), volume, 1.f);
}

void SoundController::SlidingSound::stop() {
    slide_.stop();
    whistle_.stop();
}

ClipPtr SoundController::getSpinnerSpinClip() { return getClip(SoundEffect::SPINNERSPIN); }

float SoundController::getLoopVolume(const HitObject& hitObject) { return getOutputVolume(getObjectVolume(hitObject)); }

float SoundController::getEffectLoopVolume() { return H().effectVolume() * H().masterVolume(); }

void SoundController::stopAllLoops() {
    std::vector<Loop*> loops;
    {
        std::lock_guard<std::mutex> lock(loopsMutex);
        loops.assign(activeLoops.begin(), activeLoops.end());
    }
    for (Loop* loop : loops) loop->stop();
}

// --- beatmap samples -----------------------------------------------------------------------------------------------

void SoundController::loadBeatmapSamples(const std::string& dir, const std::set<std::string>* referencedNames) {
    static const std::regex BEATMAP_SAMPLE_NAME(
        "(normal|soft|drum)-(hitnormal|hitwhistle|hitfinish|hitclap|slidertick|sliderslide|sliderwhistle)\\d*");
    clearBeatmapSamples();
    if (dir.empty() || H().soundDisabled() || !H().isDirectory(dir)) return;
    std::vector<std::string> files = H().listFiles(dir);
    std::sort(files.begin(), files.end());
    for (const std::string& name : files) {
        std::string fileName = asciiLower(name);
        auto endsWith = [&](const char* suffix) {
            std::string s(suffix);
            return fileName.size() >= s.size() && fileName.compare(fileName.size() - s.size(), s.size(), s) == 0;
        };
        if (!(endsWith(".wav") || endsWith(".mp3") || endsWith(".ogg"))) continue;
        std::string key = stripExtension(fileName);
        if (!std::regex_match(key, BEATMAP_SAMPLE_NAME) && (referencedNames == nullptr || referencedNames->count(key) == 0)) continue;
        std::string path = joinPath(dir, name);
        if (!H().isFile(path)) continue;
        // prefer .wav over .mp3/.ogg of the same name, like the skin lookup does
        if (beatmapClips.count(key) && !endsWith(".wav")) continue;
        try {
            ClipPtr clip = loadClip(absolutePath(path));
            auto it = beatmapClips.find(key);
            if (it != beatmapClips.end()) {
                ClipPtr old = it->second;
                it->second = clip;
                if (old) old->destroy();
            } else
                beatmapClips[key] = clip;
        } catch (...) {
            // a broken sample file must never break the beatmap
        }
    }
}

void SoundController::clearBeatmapSamples() {
    stopAllLoops();
    for (auto& kv : beatmapClips) {
        if (kv.second) kv.second->destroy();
    }
    beatmapClips.clear();
}

void SoundController::mute(bool m) {
    isMuted = m;
    if (m) stopAllLoops();
}

std::optional<std::string> SoundController::getCurrentFileName() { return currentFileName; }

int SoundController::getLoadingProgress() {
    if (currentFileIndex == -1) return -1;
    return currentFileIndex * 100 / static_cast<int>(SoundEffect_SIZE + (HitSound_SIZE * SampleSet_SIZE));
}

bool SoundController::playTrack(const std::string& url, const std::string&, std::function<void()> listener) {
    std::lock_guard<std::mutex> lock(trackMutex);
    // stop previous track
    if (currentTrack) {
        currentTrack->destroy();
        currentTrack.reset();
    }
    // java.net.URL(url) throws MalformedURLException without a protocol ("scheme:")
    static const std::regex HAS_PROTOCOL("^[A-Za-z][A-Za-z0-9+.\\-]*:.*");
    if (!std::regex_match(url, HAS_PROTOCOL)) {
        H().barNotification("Failed to load clip '" + url + "'.");
        return false;
    }
    currentTrack = std::make_shared<MultiClip>(url, Clip::StreamTag{});
    playClip(currentTrack, H().musicVolume() * H().masterVolume(), std::move(listener));
    return true;
}

void SoundController::stopTrack() {
    std::lock_guard<std::mutex> lock(trackMutex);
    if (currentTrack) {
        currentTrack->destroy();
        currentTrack.reset();
    }
}

void SoundController::resetForTesting() {
    stopAllLoops();
    clearBeatmapSamples();
    stopTrack();
    for (auto& c : effectClips) {
        if (c) c->destroy();
        c.reset();
    }
    for (auto& row : hitClips) {
        for (auto& c : row) {
            if (c) c->destroy();
            c.reset();
        }
    }
    if (introHit) introHit->destroy();
    if (introFinish) introFinish->destroy();
    introHit.reset();
    introFinish.reset();
    isMuted = false;
    sampleVolumeMultiplier = 1.f;
    timingPointCustomIndex = 0;
    currentFileName.reset();
    currentFileIndex = -1;
}

}  // namespace opsu
