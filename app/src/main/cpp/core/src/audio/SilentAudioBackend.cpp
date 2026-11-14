#include "opsu/audio/SilentAudioBackend.h"
#include "opsu/logging/Log.h"

#include <algorithm>
#include <chrono>
#include <mutex>

namespace opsu {

namespace {

std::function<int64_t()>& clockOverride() {
    static std::function<int64_t()> c;
    return c;
}

/** SilentAudioBackend.SilentClip: plays nothing, never calls the listener. */
class SilentClip : public ClipImplementation {
public:
    void stop() override {}
    int play(float, std::function<void()>) override { return 0; }
    void destroy() override {}
};

/** SilentAudioBackend.SilentMusic: wall-clock position, same arithmetic as the Java (float seconds). */
class SilentMusic : public AbsMusic {
public:
    explicit SilentMusic(std::string path) : path_(std::move(path)) {}

    bool setPosition(float f) override {
        basePos_ = std::max(0.f, f);
        lastStart_ = audioMillis();
        return true;
    }
    void loop() override { resume(); }
    void play() override { resume(); }
    bool playing() override { return playing_; }
    void pause() override {
        basePos_ = getPosition();
        playing_ = false;
    }
    void resume() override {
        if (!playing_) {
            lastStart_ = audioMillis();
            playing_ = true;
        }
    }
    void setVolume(float) override {}
    void stop() override { pause(); }
    float getPosition() override {
        return playing_ ? basePos_ + static_cast<float>(audioMillis() - lastStart_) / 1000.f * rate_ : basePos_;
    }
    void dispose() override { playing_ = false; }
    std::string getName() override { return path_; }
    void setPitch(float pitch) override {
        basePos_ = getPosition();
        lastStart_ = audioMillis();
        rate_ = pitch;
    }

private:
    std::string path_;
    bool playing_ = false;
    float basePos_ = 0.f;     // seconds at lastStart_
    int64_t lastStart_ = 0;   // ms
    float rate_ = 1.f;
};

std::mutex& registryMutex() {
    static std::mutex m;
    return m;
}
std::shared_ptr<IAudioBackend>& registrySlot() {
    static std::shared_ptr<IAudioBackend> b;
    return b;
}

}  // namespace

int64_t audioMillis() {
    auto& c = clockOverride();
    if (c) return c();
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void setAudioClockForTesting(std::function<int64_t()> clock) { clockOverride() = std::move(clock); }

std::unique_ptr<AbsMusic> SilentAudioBackend::createMusic(const std::string& path) {
    return std::make_unique<SilentMusic>(path);
}
std::unique_ptr<ClipImplementation> SilentAudioBackend::createClip(const std::string&) {
    return std::make_unique<SilentClip>();
}
std::unique_ptr<ClipImplementation> SilentAudioBackend::createStreamClip(const std::string&) {
    return std::make_unique<SilentClip>();
}

// ---- registry (IAudioBackend.h) -------------------------------------------------------------------------------------

void setAudioBackend(std::shared_ptr<IAudioBackend> backend) {
    std::lock_guard<std::mutex> lock(registryMutex());
    registrySlot() = std::move(backend);
}

IAudioBackend& audioBackend() {
    std::lock_guard<std::mutex> lock(registryMutex());
    auto& slot = registrySlot();
    if (!slot) {
        Log::warn("AudioBackend", "No audio backend registered - audio is silent");
        slot = std::make_shared<SilentAudioBackend>();
    }
    return *slot;
}

}  // namespace opsu
