#pragma once
#include <vector>

namespace opsu {
namespace fourresonance {

/**
 * A Project SEKAI style chart converted from an osu!mania beatmap. Port of fourresonance/PjskChart.java.
 *
 * Times are in ms on the audio timeline. Lanes use the Sekai stage unit system: the stage is
 * STAGE_WIDTH units wide and centered on 0, a note's `lane` is its center and `size` its half-width.
 *
 * timeToBeat/beatToTime forward to OsuToPjsk::timeMsToBeat/beatToTimeMs (see OsuToPjsk.h, which also holds the
 * converter that builds a chart from an osu!mania .osu file).
 * Quirk kept: `scaledAtChange` is built on the first scaledTime() call and never rebuilt, so
 * growing `timeScale` after that makes scaledTime() throw std::out_of_range (Java: AIOOBE).
 */
class PjskChart {
public:
    /** Stage width in lane units (same as NextRUSH+/Sekai: lanes span -6 to 6). */
    static constexpr float STAGE_WIDTH = 12.0f;

    /** How the tail of a hold note is judged. */
    enum class TailMode : int {
        NONE,     ///< Tail is not judged.
        RELEASE,  ///< Tail must be released on time (USC judgeType "normal").
        TRACE     ///< Tail is judged like a trace note (USC judgeType "trace").
    };

    /** A BPM segment. */
    struct BpmSegment {
        double startMs = 0;       ///< Start time (ms).
        double beatAtStart = 0;   ///< Beat number at the start of this segment.
        double beatLengthMs = 0;  ///< Milliseconds per beat.
        double bpm = 0;           ///< Beats per minute.

        BpmSegment() = default;
        BpmSegment(double startMs, double beatAtStart, double beatLengthMs, double bpm)
            : startMs(startMs), beatAtStart(beatAtStart), beatLengthMs(beatLengthMs), bpm(bpm) {}
    };

    /** A note scroll-speed change (USC timeScaleGroup entry). */
    struct TimeScaleChange {
        double beat = 0;       ///< Beat of the change.
        double timeMs = 0;     ///< Time of the change (ms).
        double timeScale = 0;  ///< Scroll speed multiplier from this point on.

        TimeScaleChange() = default;
        TimeScaleChange(double beat, double timeMs, double timeScale)
            : beat(beat), timeMs(timeMs), timeScale(timeScale) {}
    };

    /** A tap or hold note. */
    struct Note {
        int column = 0;          ///< 0-based source mania column.
        float lane = 0;          ///< Lane center in stage units.
        float size = 0;          ///< Note half-width in stage units.
        double beat = 0;         ///< Head beat (snapped to common divisions when within 1 ms of the source time).
        double endBeat = 0;      ///< Tail beat (equal to `beat` for taps).
        int timeMs = 0;          ///< Head time (ms), as written in the .osu file.
        int endTimeMs = 0;       ///< Tail time (ms); equal to `timeMs` for taps.
        bool hold = false;       ///< Whether this is a hold note.
        bool critical = false;   ///< Whether this is a critical (rainbow) note.
        TailMode tail = TailMode::RELEASE;  ///< Tail judgment (holds only).
    };

    int keyCount = 0;               ///< Number of mania columns in the source beatmap.
    float overallDifficulty = 0;    ///< Overall difficulty of the source beatmap.
    double offsetMs = 0;            ///< Time (ms) of beat 0, as the USC "offset" (converted to ms).
    std::vector<BpmSegment> bpmSegments;      ///< BPM segments (the first one starts at beat 0).
    std::vector<TimeScaleChange> timeScale;   ///< Scroll speed changes (the first one is always at beat 0).
    std::vector<Note> notes;                  ///< Notes, sorted by head time.

    /** Returns the beat at the given time (ms). */
    double timeToBeat(double timeMs) const;

    /** Returns the time (ms) of the given beat. */
    double beatToTime(double beat) const;

    /**
     * Returns the scroll position (in "scaled ms") at the given time: the integral of the scroll speed
     * multiplier over time. A note scrolls toward the judgment line according to
     * scaledTime(noteTime) - scaledTime(currentTime).
     */
    double scaledTime(double timeMs) const;

private:
    /** Cumulative scaled time at each timeScale change (built lazily). Empty = not built yet. */
    mutable std::vector<double> scaledAtChange;
};

}  // namespace fourresonance
}  // namespace opsu
