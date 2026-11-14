#pragma once
#include <array>
#include <cstddef>

namespace opsu {

/**
 * Sound effects. Port of itdelatrisu.opsu.audio.SoundEffect.
 *
 * Seam: the Java enum also held the `MultiClip clip` (getClip/setClip, from SoundComponent).
 * Clips are audio-backend objects, so they are not stored here; the future SoundController
 * keeps a `std::array<Clip, SoundEffect_SIZE>` indexed by `static_cast<size_t>(effect)`.
 * Enum order is identical to the Java declaration order (ordinal()).
 */
enum class SoundEffect : int {
    APPLAUSE,
    COMBOBREAK,
    // COUNT ("count") is commented out in the Java source
    COUNT1,
    COUNT2,
    COUNT3,
    FAIL,
    FOUNTAIN_LOOP,
    FOUNTAIN_SHOOT,
    GO,
    MENUBACK,
    MENUCLICK,
    MENUHIT,
    READY,
    SECTIONFAIL,
    SECTIONPASS,
    SHUTTER,
    SPINNERBONUS,
    SPINNEROSU,
    SPINNERSPIN,
};

/** Total number of sound effects (SoundEffect.SIZE). */
constexpr std::size_t SoundEffect_SIZE = 19;

/** SoundEffect.values(): every effect, in declaration order. */
constexpr std::array<SoundEffect, SoundEffect_SIZE> soundEffectValues() {
    return {SoundEffect::APPLAUSE, SoundEffect::COMBOBREAK, SoundEffect::COUNT1, SoundEffect::COUNT2,
            SoundEffect::COUNT3, SoundEffect::FAIL, SoundEffect::FOUNTAIN_LOOP, SoundEffect::FOUNTAIN_SHOOT,
            SoundEffect::GO, SoundEffect::MENUBACK, SoundEffect::MENUCLICK, SoundEffect::MENUHIT,
            SoundEffect::READY, SoundEffect::SECTIONFAIL, SoundEffect::SECTIONPASS, SoundEffect::SHUTTER,
            SoundEffect::SPINNERBONUS, SoundEffect::SPINNEROSU, SoundEffect::SPINNERSPIN};
}

/** SoundEffect.getFileName(): the sound file name (without extension). */
const char* getFileName(SoundEffect e);

}  // namespace opsu
