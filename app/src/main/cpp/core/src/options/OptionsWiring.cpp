// T10 milestone 3: see OptionsWiring.h.
#include "opsu/options/OptionsWiring.h"

#include "opsu/beatmap/HitObject.h"

namespace opsu {
namespace OptionsWiring {

MusicController::Host musicControllerHost(std::function<const Beatmap*()> themeBeatmap) {
    MusicController::Host h;  // members start with the MusicController defaults
    h.musicVolume = [] { return Options::getMusicVolume(); };
    h.masterVolume = [] { return Options::getMasterVolume(); };
    h.musicOffset = [] { return Options::getMusicOffset(); };
    if (themeBeatmap) h.themeBeatmap = std::move(themeBeatmap);
    return h;
}

SoundController::Host soundControllerHost() {
    SoundController::Host h;
    h.effectVolume = [] { return Options::getEffectVolume(); };
    h.hitSoundVolume = [] { return Options::getHitSoundVolume(); };
    h.masterVolume = [] { return Options::getMasterVolume(); };
    h.musicVolume = [] { return Options::getMusicVolume(); };
    h.soundDisabled = [] { return Options::isSoundDisabled(); };
    h.gameplaySoundDisabled = [] { return Options::isGameplaySoundDisabled(); };
    h.skinDirectory = [] { return Options::getSkinDir(); };
    return h;
}

void wireAudioSeams(OptionsSeams& seams) {
    seams.onVolumeAdjusted = [](float) { MusicController::onVolumeAdjusted(); };
    seams.setMusicControllerVolume = [](float v) { MusicController::setVolume(v); };
}

Beatmap::ModParams currentModParams() {
    return Beatmap::ModParams::current(Options::getFixedSpeed(), Options::getFixedCS());
}

void initHitObject(int width, int height) { HitObject::init(width, height, Options::getPlayfieldScale()); }

}  // namespace OptionsWiring
}  // namespace opsu
