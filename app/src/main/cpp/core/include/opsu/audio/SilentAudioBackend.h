#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "opsu/audio/IAudioBackend.h"

namespace opsu {

/**
 * Milliseconds on the clock the audio code uses (Java: TimeUtils.millis()). Monotonic (steady_clock), so a wall-clock
 * jump can never move the song position. Used by SilentAudioBackend and by the Music wrapper (MusicController.h).
 */
int64_t audioMillis();
/** Replaces the clock (for tests); an empty function restores the real one. Not thread-safe: call it while no audio runs. */
void setAudioClockForTesting(std::function<int64_t()> clock);

/**
 * No-sound backend (port of fluddokt.opsu.fake.SilentAudioBackend). Plays nothing, but keeps a wall-clock song position
 * (scaled by the pitch) so the gameplay logic can run and be tested without an audio engine.
 * It never fires onComplete (like the Java one: the track never ends), and createMusic never throws.
 */
class SilentAudioBackend : public IAudioBackend {
public:
    std::unique_ptr<AbsMusic> createMusic(const std::string& path) override;
    std::unique_ptr<ClipImplementation> createClip(const std::string& path) override;
    std::unique_ptr<ClipImplementation> createStreamClip(const std::string& url) override;
};

}  // namespace opsu
