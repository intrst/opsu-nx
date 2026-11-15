#pragma once
// T10 milestone 3: ready-made fillers for the places that read settings through seams. Nothing here edits those classes;
// main.cpp calls these once at start-up (exact lines in docs/agents/T10.md). Not wired on purpose: GameImage, Skin, UI,
// getThemeBeatmap() and loadSkin() (their classes are not ported / owned by another task).
#include <functional>
#include <string>

#include "opsu/audio/MusicController.h"
#include "opsu/audio/SoundController.h"
#include "opsu/beatmap/Beatmap.h"
#include "opsu/beatmap/DownloadImporter.h"
#include "opsu/options/Options.h"

namespace opsu {
namespace OptionsWiring {

/**
 * MusicController::Host with musicVolume, masterVolume and musicOffset read live from Options.
 * `themeBeatmap` is passed through (Options.getThemeBeatmap() stays a seam: build the dummy Beatmap from Options::getThemeSong()).
 * trackExists / barNotification / error keep the MusicController defaults.
 */
MusicController::Host musicControllerHost(std::function<const Beatmap*()> themeBeatmap = {});

/**
 * SoundController::Host with effectVolume, hitSoundVolume, masterVolume, musicVolume, soundDisabled, gameplaySoundDisabled and
 * skinDirectory (Options::getSkinDir(); Java: Options.getSkin().getDirectory(), the loaded skin, which Skin-less C++ cannot know).
 * layeredHitSounds (a skin.ini setting) and the file / notification / error seams keep the SoundController defaults.
 */
SoundController::Host soundControllerHost();

/**
 * Fills the two volume seams of OptionsSeams: SoundStore.setMusicVolume is dropped (OpenAL), MusicController.onVolumeAdjusted()
 * and MusicController.setVolume(master * music) are called. Use it on the struct you pass to Options::setSeams().
 */
void wireAudioSeams(OptionsSeams& seams);

/**
 * DownloadImporterEnv whose getDownloadDir / getSkinRootDir / getBeatmapDir come from Options. The two unpackers (SkinUnpacker /
 * OszUnpacker) are not Options business, so they are given as functions.
 */
class OptionsDownloadImporterEnv : public DownloadImporterEnv {
public:
    using Unpack = std::function<void(const std::string& src, const std::string& dest)>;
    OptionsDownloadImporterEnv(Unpack skins, Unpack beatmaps) : skins_(std::move(skins)), beatmaps_(std::move(beatmaps)) {}
    std::string getDownloadDir() override { return Options::getDownloadDir(); }
    std::string getSkinRootDir() override { return Options::getSkinRootDir(); }
    std::string getBeatmapDir() override { return Options::getBeatmapDir(); }
    void unpackSkins(const std::string& src, const std::string& dest) override { if (skins_) skins_(src, dest); }
    void unpackBeatmaps(const std::string& src, const std::string& dest) override { if (beatmaps_) beatmaps_(src, dest); }

private:
    Unpack skins_, beatmaps_;
};

// ---- Beatmap / HitObject / DownloadNode / BeatmapDB arguments that the Java read from Options ----
/** Beatmap::ModParams::current(Options.getFixedSpeed(), Options.getFixedCS()). */
Beatmap::ModParams currentModParams();
/** HitObject::init(width, height, Options.getPlayfieldScale()) (1.0 on desktop, 1.6 on small screens by default). */
void initHitObject(int width, int height);
/** beatmap.getTitle(Options.useUnicodeMetadata()) / getArtist / toString. */
inline const std::string& title(const Beatmap& b) { return b.getTitle(Options::useUnicodeMetadata()); }
inline const std::string& artist(const Beatmap& b) { return b.getArtist(Options::useUnicodeMetadata()); }
inline std::string toString(const Beatmap& b) { return b.toString(Options::useUnicodeMetadata()); }
/**
 * Arguments for DownloadNode::createDownload(source, importDir, useUnicode, ...) and the `useUnicode` parameter of BeatmapDB
 * (setStars, updatePlayStatistics, ...): Options.getImportDir() / Options.useUnicodeMetadata().
 */
inline std::string importDir() { return Options::getImportDir(); }
inline bool useUnicodeMetadata() { return Options::useUnicodeMetadata(); }

}  // namespace OptionsWiring
}  // namespace opsu
