#pragma once
#include <memory>
#include <string>

#include "opsu/audio/AbsMusic.h"
#include "opsu/audio/ClipImplementation.h"

namespace opsu {

/**
 * The game's single audio engine (port of fluddokt.opsu.fake.AudioBackend).
 * On Android this is BASS (platform/audio/BassBackend, task T2); in the core tests it is
 * SilentAudioBackend. Every Music / Clip goes through it.
 */
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    /** Streaming music track. Throws java::IOException (opsu/util/JavaIO.h) if the file cannot be
     *  opened / decoded. The Music wrapper sets onComplete / onRequestSync on the result. */
    virtual std::unique_ptr<AbsMusic> createMusic(const std::string& path) = 0;
    /** Short, fully loaded effect. Never null: a failed load is a silent clip. */
    virtual std::unique_ptr<ClipImplementation> createClip(const std::string& path) = 0;
    /** One-shot clip streamed from a URL (beatmap previews). Never null. */
    virtual std::unique_ptr<ClipImplementation> createStreamClip(const std::string& url) = 0;
};

/** Registers the backend (nullptr clears it). Thread-safe. */
void setAudioBackend(std::shared_ptr<IAudioBackend> backend);
/** The active backend; if none is registered, a SilentAudioBackend is created, registered and returned
 *  (Java logs a warning once). Never fails. */
IAudioBackend& audioBackend();

}  // namespace opsu
