#include "opsu/audio/MultiClip.h"

#include "opsu/audio/IAudioBackend.h"

namespace opsu {

Clip::Clip(const std::string& path) : sound_(audioBackend().createClip(path)) {}

Clip::Clip(const std::string& url, StreamTag) : sound_(audioBackend().createStreamClip(url)) {}

Clip::~Clip() {
    if (sound_) sound_->destroy();
}

void Clip::start(float volume, std::function<void()> onEnd) { id_ = sound_->play(volume, std::move(onEnd)); }

int Clip::startLoop(float volume, float frequencyRatio) { return sound_->playLoop(volume, frequencyRatio); }

void Clip::setLoopVolume(int loopId, float volume) {
    if (loopId != 0) sound_->setChannelVolume(loopId, volume);
}

void Clip::setLoopFrequency(int loopId, float frequencyRatio) {
    if (loopId != 0) sound_->setChannelFrequency(loopId, frequencyRatio);
}

void Clip::stopLoop(int loopId) {
    if (loopId != 0) sound_->stopChannel(loopId);
}

void Clip::destroy() { sound_->destroy(); }

void Clip::stop() { sound_->stop(); }

}  // namespace opsu
