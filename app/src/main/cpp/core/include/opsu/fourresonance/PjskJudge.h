#pragma once

namespace opsu {
namespace fourresonance {

/**
 * Judgment windows and score model for the PJSK mode. Port of fourresonance/PjskJudge.java
 * (the Java package is spelled "fourresonace" in the source; the directory is "fourresonance").
 *
 * Windows come from sekai/lib/buckets.py: defined in 60 fps frames, converted to ms here.
 * A judgment is made on diff = actualTime - targetTime (negative = early); windows are nested
 * (perfect inside great inside good), inclusive.
 *
 * Seam: the Java `Judgment` enum carried a `multiplier` field; here it is `multiplier(Judgment)`.
 * The window constants are `inline constexpr` (no static-init-order issues, header only).
 */

/** A judgment result. Declaration order = Java ordinal(). */
enum class Judgment : int { PERFECT, GREAT, GOOD, MISS };

/** Share of a note's score weight earned by this judgment (the engine's perfect/great/good multipliers). */
constexpr double multiplier(Judgment j) {
    switch (j) {
        case Judgment::PERFECT: return 1.0;
        case Judgment::GREAT:   return 0.7;
        case Judgment::GOOD:    return 0.5;
        case Judgment::MISS:    return 0.0;
    }
    return 0.0;
}

/** Java Enum.name(). */
constexpr const char* name(Judgment j) {
    switch (j) {
        case Judgment::PERFECT: return "PERFECT";
        case Judgment::GREAT:   return "GREAT";
        case Judgment::GOOD:    return "GOOD";
        case Judgment::MISS:    return "MISS";
    }
    return "";
}

/** A set of asymmetric judgment windows, in ms (all values are positive distances from the target time). */
struct Window {
    double perfectEarly, perfectLate;
    double greatEarly, greatLate;
    double goodEarly, goodLate;

    constexpr Window(double pe, double pl, double ge, double gl, double oe, double ol)
        : perfectEarly(pe), perfectLate(pl), greatEarly(ge), greatLate(gl), goodEarly(oe), goodLate(ol) {}

    /** Judges an input at diffMs (actual - target, negative = early). */
    constexpr Judgment judge(double diffMs) const {
        if (diffMs >= -perfectEarly && diffMs <= perfectLate) return Judgment::PERFECT;
        if (diffMs >= -greatEarly && diffMs <= greatLate) return Judgment::GREAT;
        if (diffMs >= -goodEarly && diffMs <= goodLate) return Judgment::GOOD;
        return Judgment::MISS;
    }

    /** Whether an input at diffMs is inside the outermost (good) window. */
    constexpr bool inRange(double diffMs) const { return diffMs >= -goodEarly && diffMs <= goodLate; }
};

namespace detail {
constexpr double FRAME_MS = 1000.0 / 60;

/** {early, late} in frames (Java double[2]). */
struct Frames { double early, late; };
constexpr Frames f(double both) { return {both, both}; }
constexpr Frames f(double early, double late) { return {early, late}; }

/** Java frames(perfect, great, good). */
constexpr Window frames(Frames perfect, Frames great, Frames good) {
    return Window(perfect.early * FRAME_MS, perfect.late * FRAME_MS,
                  great.early * FRAME_MS, great.late * FRAME_MS,
                  good.early * FRAME_MS, good.late * FRAME_MS);
}
/** Java frames(perfect, null, null): great and good default to the previous window. */
constexpr Window frames(Frames perfect) { return frames(perfect, perfect, perfect); }
}  // namespace detail

/** Normal tap / hold head: perfect 2.5, great 5, good 6.5 frames each side. */
inline constexpr Window TAP_NORMAL = detail::frames(detail::f(2.5), detail::f(5), detail::f(6.5));
/** Critical tap / hold head: perfect 3.3, great 4.5, good 6.5 frames each side. */
inline constexpr Window TAP_CRITICAL = detail::frames(detail::f(3.3), detail::f(4.5), detail::f(6.5));
/** Hold tail released on time (normal and critical share the same window). */
inline constexpr Window SLIDE_END = detail::frames(detail::f(3.5, 4), detail::f(6.5, 8), detail::f(7.5, 8.5));
/** Hold tail judged like a trace: anything from 6.5 frames early to 8 frames late is perfect. */
inline constexpr Window SLIDE_END_TRACE = detail::frames(detail::f(6.5, 8));

/** Score weight of every judged note ("unweighted" score mode, the engine's default). */
inline constexpr int NOTE_WEIGHT = 10;

/** Maximum score. */
inline constexpr int MAX_SCORE = 1000000;

}  // namespace fourresonance
}  // namespace opsu
