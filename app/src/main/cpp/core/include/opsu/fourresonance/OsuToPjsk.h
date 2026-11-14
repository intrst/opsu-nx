#pragma once
#include <array>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>
#include "opsu/fourresonance/PjskChart.h"

namespace opsu {
namespace fourresonance {

/**
 * osu!mania (.osu, Mode 3) to PjskChart converter. Port of fourresonance/OsuToPjsk.java (itself a Java port of
 * om2pjsk-usc: same BPM segment model, 1 ms beat snapping, column -> lane/size mapping, tail modes, Tinged
 * Columns and SV-to-timeScale conversion). The result is kept in memory.
 *
 * Deviations from the Python reference (kept from the Java): red timing points with a non-positive beat length
 * are ignored; a hold without a parseable end time is a tap; notes are sorted by head time.
 *
 * C++ differences:
 *  - java.io.Reader -> std::istream (bytes; the parser only looks at ASCII, so UTF-8 needs no decoding).
 *    Line ends are \n, \r or \r\n like BufferedReader.readLine. A stream that is already failed (failbit / badbit, e.g. an ifstream that
 *    did not open) or whose buffer throws while reading gives java::IOException (what the Reader would throw).
 *  - null chart -> std::nullopt.
 *  - A key count above MAX_KEYS (1 << 20) throws std::length_error: the Java would run out of memory
 *    (OutOfMemoryError) allocating its per-column arrays, e.g. for "CircleSize:1e12".
 *  - String.toLowerCase() on the (ASCII-only) keys is locale independent here.
 *  - Double.parseDouble: java::parseDouble (no hexadecimal floating-point literals, like java::parseFloat).
 */
namespace OsuToPjsk {

/** Default limit of fingers needed at once (two thumbs), so charts stay playable on a phone. */
constexpr int DEFAULT_MAX_SIMULTANEOUS = 2;
/** Largest key count convert() accepts (see above). */
constexpr int MAX_KEYS = 1 << 20;

/** Time (ms) -> beat. Empty segment list = fixed 500 ms/beat. Uses a 1e-6 ms tolerance at segment starts. */
double timeMsToBeat(double timeMs, const std::vector<PjskChart::BpmSegment>& segments);

/** Beat -> time (ms). Empty segment list = fixed 500 ms/beat. */
double beatToTimeMs(double beat, const std::vector<PjskChart::BpmSegment>& segments);

/**
 * Parses an osu!mania beatmap and converts it.
 * @param in the .osu text
 * @param tailMode how hold tails are judged
 * @param tingedColumns whether notes on Tinged Columns become critical notes
 * @param minHoldMs long notes shorter than this become taps (0 keeps every long note)
 * @param maxSimultaneous at most this many fingers are needed at once (0 or less disables the limit); extra
 *        notes of a chord are dropped, keeping the ones farthest apart
 * @return the chart, or nullopt if the file is not a usable mania chart (not Mode 3, no columns, or one object or fewer)
 * @throws java::IOException if the stream is not readable (failed / bad) or its buffer throws while reading
 */
std::optional<PjskChart> convert(std::istream& in, PjskChart::TailMode tailMode, bool tingedColumns,
                                 int minHoldMs = 0, int maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS);

/** Same as convert(std::istream&, ...) for text already in memory. */
std::optional<PjskChart> convertString(const std::string& osuText, PjskChart::TailMode tailMode, bool tingedColumns,
                                       int minHoldMs = 0, int maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS);

/**
 * Returns the default (undrifted) center and half-size of a 0-based mania column: {lane, size}.
 * @throws std::out_of_range if col is not in [0, keyCount)  (Java: ArrayIndexOutOfBoundsException)
 * @throws java::NegativeArraySizeException if keyCount < 0
 */
std::array<float, 2> columnToLaneSize(int keyCount, int col);

/** Whether the 0-based column is part of Tinged Columns for the given key count (only 4K..7K have any). */
bool isTingedColumn(int keyCount, int col);

/**
 * PJSK-style lane placement. The Sekai stage is 12 one-unit lanes wide and notes sit exactly on that grid: 2 units
 * wide for up to 5 columns, 1 unit wide for 6 and more. A column is only a starting hint: every time a column has
 * been idle for MOVE_AFTER_MS its next note may shift by one lane either way (chosen deterministically from the
 * note's time), as long as columns keep their order and never overlap. Jacks and streams stay put, chords always get
 * separate lanes, and holds keep their lane until the tail.
 */
class LaneLayout {
public:
    static constexpr int MOVE_AFTER_MS = 350;

    /** @throws java::NegativeArraySizeException if keys < 0 */
    explicit LaneLayout(int keys);

    /**
     * Places a note of column `col` at `time` (ms) lasting until `endTime`; returns {lane, size}.
     * Stateful: call in note-time order. @throws std::out_of_range if col is not in [0, keys)
     */
    std::array<float, 2> place(int col, int time, int endTime);

private:
    int keys_;
    int positions_;
    double half_;
    std::vector<int> base_, cur_, busyUntil_, lastTime_;
};

/** Java's private helpers, public here so the tests can reach them. Not part of the stable API. */
namespace detail {

struct Tp {
    double time = 0;
    double beatLength = 0;
    int uninherited = 1;

    bool isRed() const { return uninherited == 1; }
    double bpm() const;
    double svMultiplier() const;
};

struct Ho {
    int x = 0, time = 0, type = 0;
    bool hasEnd = false;
    int endTime = 0;

    bool isHold() const { return (type & 128) != 0 && hasEnd; }
    int lane(int keyCount) const;
};

struct Parsed {
    int mode = 0;
    double circleSize = 0;
    double overallDifficulty = 0;
    std::vector<Tp> timingPoints;
    std::vector<Ho> hitObjects;

    int keyCount() const;
};

/** Lenient .osu parser (osu_parser.py): bad lines are skipped, only [General] Mode and [Difficulty] are read. */
Parsed parse(std::istream& in);

/** Drops hit objects so that at most `cap` fingers are needed at any moment (cap <= 0: no-op). */
void limitSimultaneous(Parsed& p, int cap);

/** Snaps to common beat subdivisions, only when within 1 ms of the source time. */
double noteTimeMsToBeat(double timeMs, const std::vector<PjskChart::BpmSegment>& segments);

}  // namespace detail

}  // namespace OsuToPjsk

}  // namespace fourresonance
}  // namespace opsu
