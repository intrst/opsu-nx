#pragma once
#include "opsu/GameMod.h"

namespace opsu {

/**
 * Playback speeds for replays. Port of itdelatrisu.opsu.replay.PlaybackSpeed (logic half).
 * The per-speed MenuButton/GameImage and init(width,height) are not ported yet (UI).
 */
enum class PlaybackSpeed { NORMAL = 0, DOUBLE = 1, HALF = 2 };

inline float getModifier(PlaybackSpeed s) {
    switch (s) {
        case PlaybackSpeed::NORMAL: return 1.f;
        case PlaybackSpeed::DOUBLE: return 2.f;
        default: return 0.5f;
    }
}

/** Returns the next playback speed (skips DOUBLE while DoubleTime/Nightcore is active). */
inline PlaybackSpeed next(PlaybackSpeed s) {
    PlaybackSpeed n = static_cast<PlaybackSpeed>((static_cast<int>(s) + 1) % 3);
    if ((GameMod::DOUBLE_TIME.isActive() || GameMod::NIGHTCORE.isActive()) && n == PlaybackSpeed::DOUBLE)
        n = next(n);
    return n;
}

}  // namespace opsu
