#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace opsu {

/**
 * Hit sounds. Port of itdelatrisu.opsu.audio.HitSound.
 *
 * Seam (same as SoundEffect): the Java enum also held `HashMap<SampleSet, MultiClip> clips` (getClip/setClip). Clips are
 * audio-backend objects, so they are not stored here: the future SoundController keeps a
 * `std::array<std::array<Clip, SampleSet_SIZE>, HitSound_SIZE>` indexed by the two enums. Java `getClip()` (no argument) =
 * "the clip of the current sample set, or null when none is set": use `activeSampleSet()` for that lookup.
 * Enum order is identical to the Java declaration order (ordinal()).
 */
enum class HitSound : int { CLAP, FINISH, NORMAL, WHISTLE, SLIDERSLIDE, SLIDERTICK, SLIDERWHISTLE };

/** Total number of hit sounds (HitSound.SIZE). */
constexpr std::size_t HitSound_SIZE = 7;

constexpr std::array<HitSound, HitSound_SIZE> hitSoundValues() {
    return {HitSound::CLAP, HitSound::FINISH, HitSound::NORMAL, HitSound::WHISTLE,
            HitSound::SLIDERSLIDE, HitSound::SLIDERTICK, HitSound::SLIDERWHISTLE};
}

/** HitSound.getFileName(): the sound file name (without extension). */
const char* getFileName(HitSound h);

/** Sound sample sets (HitSound.SampleSet). TAIKO is commented out in the Java source. */
enum class SampleSet : int { NORMAL, SOFT, DRUM };

/** Total number of sample sets (SampleSet.SIZE). */
constexpr std::size_t SampleSet_SIZE = 3;

constexpr std::array<SampleSet, SampleSet_SIZE> sampleSetValues() {
    return {SampleSet::NORMAL, SampleSet::SOFT, SampleSet::DRUM};
}

/** SampleSet.getName(): "normal", "soft", "drum". */
const char* getName(SampleSet s);
/** SampleSet.getIndex(): 1, 2, 3 (0 means "auto" and is not a sample set). */
int getIndex(SampleSet s);

namespace hitsound {

/**
 * HitSound.setDefaultSampleSet(String): "Normal"/"Soft"/"Drum" (case-insensitive, ASCII); anything else, including "auto",
 * selects NORMAL.
 */
void setDefaultSampleSet(const std::string& sampleSet);
/**
 * HitSound.setDefaultSampleSet(byte): 1 normal, 2 soft, 3 drum; anything else (0 = auto, invalid) selects NORMAL.
 * Named differently from the String overload so that a literal 0 cannot be mistaken for a null pointer.
 */
void setDefaultSampleSetType(int8_t sampleType);
/** The current default sample set (initially NORMAL). */
SampleSet defaultSampleSet();

/**
 * HitSound.resolveSampleSet(byte): the sample set with that index, otherwise the current default sample set
 * (0 or invalid = "whatever the current timing point / beatmap default is", like osu!lazer).
 */
SampleSet resolveSampleSet(int8_t sampleType);

/**
 * HitSound.setSampleSet(byte) (@Deprecated in Java: global, not thread-safe state; kept because the Java code still calls it):
 * sets the sample set used by Java `getClip()`: the one with that index, otherwise the current default sample set.
 */
void setSampleSet(int8_t sampleType);
/** The set Java `getClip()` would read: nullopt (Java null) until setSampleSet() has been called. */
std::optional<SampleSet> activeSampleSet();

/** Restores the initial static state (default NORMAL, active none). No Java counterpart; for tests. */
void resetForTesting();

}  // namespace hitsound
}  // namespace opsu
