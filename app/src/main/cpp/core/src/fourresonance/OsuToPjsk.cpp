#include "opsu/fourresonance/OsuToPjsk.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <istream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"
#include "opsu/util/JavaMath.h"

namespace opsu {
namespace fourresonance {
namespace OsuToPjsk {

namespace {

using BpmSegment = PjskChart::BpmSegment;
using Note = PjskChart::Note;
using TailMode = PjskChart::TailMode;
using TimeScaleChange = PjskChart::TimeScaleChange;
using detail::Ho;
using detail::Parsed;
using detail::Tp;

constexpr double FLOAT_TOLERANCE = 1e-6;
constexpr double NOTE_SNAP_TOLERANCE_MS = 1.0;
constexpr int NOTE_SNAP_DIVISIONS[] = {1, 2, 3, 4, 6, 8, 12, 16};

/** Chord window: notes whose head times are at most this far apart (ms) from the previous one are one chord. */
constexpr int CHORD_WINDOW_MS = 12;

/** Tinged Columns (critical notes) by key count: 4K: 1/4, 5K: 2/4, 6K: 2/5, 7K: 2/4/6. */
const std::vector<std::vector<int>> TINGED_COLUMNS = {
    /* 4K */ {0, 3},
    /* 5K */ {1, 3},
    /* 6K */ {1, 4},
    /* 7K */ {1, 3, 5},
};

constexpr int INT_MIN_ = std::numeric_limits<int>::min();

/** Java int subtraction (wraps instead of being undefined behaviour). */
inline int wrapSub(int a, int b) { return static_cast<int>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
inline int wrapAdd(int a, int b) { return static_cast<int>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }

/** Math.floorMod(int, int) for a positive divisor. */
inline int floorMod(int x, int m) { int r = x % m; return r < 0 ? r + m : r; }

bool startsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

/** key.matches("[A-Za-z0-9]+") */
bool isAlnumKey(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return false;
    return true;
}

std::string lowerAscii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// ---------------------------------------------------------------------------------------------
// Parser (osu_parser.py)
// ---------------------------------------------------------------------------------------------
void parseTimingPoint(Parsed& chart, const std::string& line) {
    std::vector<std::string> parts = java::splitAll(line, ',');
    if (parts.size() < 7)
        return;
    try {
        Tp tp;
        tp.time = java::parseDouble(java::trim(parts[0]));
        tp.beatLength = java::parseDouble(java::trim(parts[1]));
        std::string u = java::trim(parts[6]);
        tp.uninherited = u.empty() ? 1 : java::parseInt(u);
        chart.timingPoints.push_back(tp);
    } catch (const java::NumberFormatException&) {
        // skipped, like the Python parser
    }
}

/** Format: x,y,time,type,hitSound,objectParams,hitSample (holds: endTime:hitSample). */
void parseHitObject(Parsed& chart, const std::string& line) {
    std::vector<std::string> parts = java::splitAll(line, ',');
    if (parts.size() < 6)
        return;
    try {
        Ho ho;
        ho.x = java::parseInt(java::trim(parts[0]));
        java::parseInt(java::trim(parts[1]));  // y (validated, unused)
        ho.time = java::parseInt(java::trim(parts[2]));
        ho.type = java::parseInt(java::trim(parts[3]));
        java::parseInt(java::trim(parts[4]));  // hitSound (validated, unused)

        if ((ho.type & 128) != 0) {
            // remainder after the 5th comma: "endTime:hitSample,..."
            std::string remainder = parts[5];
            for (size_t i = 6; i < parts.size(); i++)
                remainder.append(1, ',').append(parts[i]);
            size_t colon = remainder.find(':');
            if (colon != std::string::npos && colon > 0) {
                try {
                    ho.endTime = java::parseInt(java::trim(remainder.substr(0, colon)));
                    ho.hasEnd = true;
                } catch (const java::NumberFormatException&) {
                }
            }
        }
        chart.hitObjects.push_back(ho);
    } catch (const java::NumberFormatException&) {
        // skipped, like the Python parser
    }
}

void parseKeyValue(Parsed& chart, const std::string& section, const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0)
        return;
    std::string key = java::trim(line.substr(0, colon));
    if (!isAlnumKey(key))
        return;
    std::string value = java::trim(line.substr(colon + 1));
    key = lowerAscii(key);
    try {
        if (section == "General") {
            if (key == "mode")
                chart.mode = value.empty() ? 0 : java::parseInt(value);
        } else if (section == "Difficulty") {
            if (key == "circlesize")
                chart.circleSize = value.empty() ? 0.0 : java::parseDouble(value);
            else if (key == "overalldifficulty")
                chart.overallDifficulty = value.empty() ? 0.0 : java::parseDouble(value);
        }
    } catch (const java::NumberFormatException&) {
        // ignore malformed values
    }
}

// ---------------------------------------------------------------------------------------------
// Timing model (osu_to_usc.py)
// ---------------------------------------------------------------------------------------------
std::vector<BpmSegment> buildBpmSegments(const std::vector<Tp>& reds) {
    std::vector<BpmSegment> segments;
    std::vector<Tp> sorted;
    for (const Tp& tp : reds)
        if (tp.beatLength > 0)
            sorted.push_back(tp);
    if (sorted.empty()) {
        segments.emplace_back(0.0, 0.0, 500.0, 120.0);  // default: 120 BPM at time 0
        return segments;
    }
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Tp& a, const Tp& b) { return java::compare(a.time, b.time) < 0; });
    for (size_t i = 0; i < sorted.size(); i++) {
        const Tp& tp = sorted[i];
        double beatAtStart = 0.0;
        if (i > 0) {
            const BpmSegment& prev = segments.back();
            beatAtStart = prev.beatAtStart + (tp.time - prev.startMs) / prev.beatLengthMs;
        }
        segments.emplace_back(tp.time, beatAtStart, tp.beatLength, tp.bpm());
    }
    return segments;
}

std::map<int, double> buildNoteBeats(const Parsed& chart, const std::vector<BpmSegment>& segments) {
    std::set<int> timeSet;
    for (const Ho& ho : chart.hitObjects) {
        timeSet.insert(ho.time);
        if (ho.isHold())
            timeSet.insert(ho.endTime);
    }
    const int n = static_cast<int>(timeSet.size());
    std::vector<int> times(timeSet.begin(), timeSet.end());
    std::vector<double> original(n), snapped(n);
    for (int i = 0; i < n; i++) {
        original[i] = timeMsToBeat(times[i], segments);
        snapped[i] = detail::noteTimeMsToBeat(times[i], segments);
    }

    // Share snaps across chords and undo collisions, including LN head/tail pairs.
    // A restored timestamp can conflict with a neighbor, so repeat until stable.
    while (true) {
        std::set<int> restore;
        for (int i = 0; i + 1 < n; i++) {
            if (original[i] < original[i + 1] && snapped[i] >= snapped[i + 1]) {
                restore.insert(i);
                restore.insert(i + 1);
            }
        }
        bool changed = false;
        for (int i : restore) {
            if (snapped[i] != original[i]) {
                snapped[i] = original[i];
                changed = true;
            }
        }
        if (!changed)
            break;
    }

    std::map<int, double> result;
    for (int i = 0; i < n; i++)
        result[times[i]] = snapped[i];
    return result;
}

double bpmAtTime(double timeMs, const std::vector<BpmSegment>& segments) {
    if (segments.empty())
        return 120.0;
    const BpmSegment* seg = &segments[0];
    for (const BpmSegment& s : segments) {
        if (s.startMs <= timeMs + FLOAT_TOLERANCE)
            seg = &s;
        else
            break;
    }
    return seg->bpm;
}

/** Groups timing points that begin at effectively the same time. */
std::vector<std::vector<Tp>> timingPointGroups(const std::vector<Tp>& timingPoints) {
    std::vector<Tp> sorted = timingPoints;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Tp& a, const Tp& b) { return java::compare(a.time, b.time) < 0; });
    std::vector<std::vector<Tp>> groups;
    for (const Tp& tp : sorted) {
        if (!groups.empty() && std::fabs(tp.time - groups.back()[0].time) < FLOAT_TOLERANCE)
            groups.back().push_back(tp);
        else
            groups.push_back(std::vector<Tp>{tp});
    }
    return groups;
}

/** Returns the effective osu!mania SV after a same-time timing group. */
double effectiveSv(const std::vector<Tp>& group) {
    const Tp* lastGreen = nullptr;
    for (const Tp& tp : group)
        if (!tp.isRed())
            lastGreen = &tp;
    return lastGreen ? lastGreen->svMultiplier() : 1.0;
}

/** Builds scroll speed changes from the normalized effective osu!mania scroll speed (BPM x SV). */
std::vector<TimeScaleChange> buildTimeScale(const Parsed& p, const std::vector<BpmSegment>& segments) {
    std::vector<TimeScaleChange> changes;
    changes.emplace_back(0.0, beatToTimeMs(0.0, segments), 1.0);  // base timeScale = 1 at beat 0

    std::vector<std::array<double, 2>> effective;  // {time, bpm * sv}
    for (const std::vector<Tp>& group : timingPointGroups(p.timingPoints)) {
        double time = group[0].time;
        double bpm = bpmAtTime(time, segments);
        effective.push_back({time, bpm * effectiveSv(group)});
    }
    if (effective.empty())
        return changes;

    double baseScroll = effective[0][1];
    if (std::fabs(baseScroll) < FLOAT_TOLERANCE)
        baseScroll = 120.0;

    for (const auto& e : effective) {
        double timeScale = e[1] / baseScroll;
        double beat = timeMsToBeat(e[0], segments);
        TimeScaleChange& last = changes.back();
        if (std::fabs(beat - last.beat) < FLOAT_TOLERANCE)
            last.timeScale = timeScale;
        else if (std::fabs(timeScale - last.timeScale) < FLOAT_TOLERANCE)
            continue;
        else
            changes.emplace_back(beat, beatToTimeMs(beat, segments), timeScale);
    }
    return changes;
}

// ---------------------------------------------------------------------------------------------
// Chart builder (USCBuilder)
// ---------------------------------------------------------------------------------------------
PjskChart build(const Parsed& p, TailMode tailMode, bool tingedColumns) {
    PjskChart out;
    out.keyCount = p.keyCount();
    out.overallDifficulty = static_cast<float>(p.overallDifficulty);

    // BPM segments, extended backward by whole beats if notes start before the first red line
    std::vector<Tp> reds;
    for (const Tp& tp : p.timingPoints)
        if (tp.isRed())
            reds.push_back(tp);
    std::vector<BpmSegment> segments = buildBpmSegments(reds);
    const BpmSegment first = segments[0];  // copy: the vector is modified below
    double firstNoteMs = first.startMs;
    if (!p.hitObjects.empty()) {
        firstNoteMs = std::numeric_limits<double>::max();
        for (const Ho& ho : p.hitObjects)
            firstNoteMs = java::min(firstNoteMs, static_cast<double>(ho.time));
    }
    if (firstNoteMs < first.startMs) {
        // Extend backward by whole beats, preserving note and hidden-tick grid phase.
        int leadBeats = wrapAdd(java::toInt(std::floor((first.startMs - firstNoteMs) / first.beatLengthMs)), 1);
        for (BpmSegment& s : segments)
            s.beatAtStart += leadBeats;
        segments.insert(segments.begin(), BpmSegment(first.startMs - leadBeats * first.beatLengthMs, 0.0,
                                                     first.beatLengthMs, first.bpm));
    }
    out.bpmSegments = segments;
    out.offsetMs = segments[0].startMs;

    // scroll speed (timeScaleGroup)
    out.timeScale = buildTimeScale(p, segments);

    // notes
    const int keyCount = out.keyCount;
    std::map<int, double> noteBeats = buildNoteBeats(p, segments);
    std::vector<Ho> ordered = p.hitObjects;
    std::stable_sort(ordered.begin(), ordered.end(), [](const Ho& a, const Ho& b) { return a.time < b.time; });
    LaneLayout layout(keyCount);
    for (const Ho& ho : ordered) {
        int col = ho.lane(keyCount);
        std::array<float, 2> ls = layout.place(col, ho.time, ho.isHold() ? ho.endTime : ho.time);
        Note n;
        n.column = col;
        n.lane = ls[0];
        n.size = ls[1];
        n.beat = noteBeats.at(ho.time);
        n.timeMs = ho.time;
        n.critical = tingedColumns && isTingedColumn(keyCount, col);
        if (ho.isHold()) {
            n.hold = true;
            n.endBeat = java::max(n.beat, noteBeats.at(ho.endTime));
            n.endTimeMs = java::max(ho.time, ho.endTime);
            n.tail = tailMode;
        } else {
            n.endBeat = n.beat;
            n.endTimeMs = ho.time;
        }
        out.notes.push_back(n);
    }
    std::stable_sort(out.notes.begin(), out.notes.end(), [](const Note& a, const Note& b) { return a.timeMs < b.timeMs; });
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------------------------
double timeMsToBeat(double timeMs, const std::vector<PjskChart::BpmSegment>& segments) {
    if (segments.empty()) return timeMs / 500.0;
    const PjskChart::BpmSegment* seg = &segments[0];
    for (const PjskChart::BpmSegment& s : segments) {
        if (s.startMs <= timeMs + FLOAT_TOLERANCE) seg = &s;
        else break;
    }
    return seg->beatAtStart + (timeMs - seg->startMs) / seg->beatLengthMs;
}

double beatToTimeMs(double beat, const std::vector<PjskChart::BpmSegment>& segments) {
    if (segments.empty()) return beat * 500.0;
    const PjskChart::BpmSegment* seg = &segments[0];
    for (const PjskChart::BpmSegment& s : segments) {
        if (s.beatAtStart <= beat) seg = &s;
        else break;
    }
    return seg->startMs + (beat - seg->beatAtStart) * seg->beatLengthMs;
}

bool isTingedColumn(int keyCount, int col) {
    int i = keyCount - 4;
    if (i < 0 || i >= static_cast<int>(TINGED_COLUMNS.size()))
        return false;
    for (int c : TINGED_COLUMNS[i])
        if (c == col)
            return true;
    return false;
}

// ---------------------------------------------------------------------------------------------
// LaneLayout
// ---------------------------------------------------------------------------------------------
LaneLayout::LaneLayout(int keys) : keys_(keys) {
    if (keys < 0)
        throw java::NegativeArraySizeException(std::to_string(keys));
    half_ = keys <= 5 ? 1.0 : 0.5;
    positions_ = static_cast<int>(java::round(PjskChart::STAGE_WIDTH - 2 * half_)) + 1;  // 11 (width 2) or 12 (width 1)
    base_.assign(keys, 0);
    cur_.assign(keys, 0);
    busyUntil_.assign(keys, 0);
    lastTime_.assign(keys, 0);
    for (int c = 0; c < keys; c++) {
        base_[c] = keys == 1 ? (positions_ - 1) / 2
                             : static_cast<int>(java::round(c * (positions_ - 1.0) / (keys - 1)));
        cur_[c] = base_[c];
        busyUntil_[c] = lastTime_[c] = INT_MIN_ / 2;
    }
}

std::array<float, 2> LaneLayout::place(int col, int time, int endTime) {
    int gap = static_cast<int>(java::round(2 * half_));  // minimum distance between neighbouring columns
    if (wrapSub(time, std::max(busyUntil_.at(col), lastTime_.at(col))) >= MOVE_AFTER_MS && time > INT_MIN_ / 4) {
        // Java int arithmetic: wraps; hex literals are the same bit patterns as the unsigned constants below
        uint32_t h = static_cast<uint32_t>(time) * 0x9E3779B1u + static_cast<uint32_t>(col) * 0x85EBCA6Bu;
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        int step = floorMod(static_cast<int>(h), 3) - 1;  // -1, 0 or +1
        int want = std::max(0, std::min(positions_ - 1, base_[col] + step));
        bool ok = true;
        if (col > 0 && want - cur_[col - 1] < gap) ok = false;
        if (col < keys_ - 1 && cur_[col + 1] - want < gap) ok = false;
        if (ok)
            cur_[col] = want;
    }
    lastTime_[col] = time;
    busyUntil_[col] = std::max(busyUntil_[col], endTime);
    float lane = static_cast<float>(-PjskChart::STAGE_WIDTH / 2 + half_ + cur_[col]);
    return {lane, static_cast<float>(half_)};
}

std::array<float, 2> columnToLaneSize(int keyCount, int col) {
    return LaneLayout(keyCount).place(col, INT_MIN_ / 2, INT_MIN_ / 2);
}

// ---------------------------------------------------------------------------------------------
// detail
// ---------------------------------------------------------------------------------------------
namespace detail {

double Tp::bpm() const { return (isRed() && beatLength > 0) ? 60000.0 / beatLength : 0.0; }
double Tp::svMultiplier() const { return (!isRed() && beatLength != 0) ? 100.0 / std::fabs(beatLength) : 1.0; }

int Ho::lane(int keyCount) const {
    int col = java::toInt(std::floor(static_cast<double>(x) * keyCount / 512));
    return std::max(0, std::min(keyCount - 1, col));
}

int Parsed::keyCount() const { return java::toInt(std::floor(circleSize)); }

Parsed parse(std::istream& in) {
    Parsed chart;
    std::string data;
    try {
        if (in.fail())
            throw java::IOException("stream is not readable");  // e.g. an ifstream that failed to open (Java: FileNotFoundException earlier)
        data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    } catch (const java::IOException&) {
        throw;
    } catch (const std::exception& e) {
        throw java::IOException(e.what());  // a stream buffer that throws while reading = the Reader's IOException
    }

    bool hasSection = false;
    std::string section;
    size_t pos = 0;
    while (pos < data.size()) {
        // BufferedReader.readLine: a line ends at \n, \r or \r\n
        size_t end = data.find_first_of("\r\n", pos);
        std::string raw;
        if (end == std::string::npos) {
            raw = data.substr(pos);
            pos = data.size();
        } else {
            raw = data.substr(pos, end - pos);
            pos = end + 1;
            if (data[end] == '\r' && pos < data.size() && data[pos] == '\n')
                pos++;
        }
        std::string line = java::trim(raw);
        if (line.empty() || startsWith(line, "//"))
            continue;

        // section header: [Name]
        if (line.size() >= 3 && line.front() == '[' && line.back() == ']' && line.find(']') == line.size() - 1) {
            section = java::trim(line.substr(1, line.size() - 2));
            hasSection = true;
            continue;
        }
        if (!hasSection)
            continue;

        if (section == "HitObjects")
            parseHitObject(chart, line);
        else if (section == "TimingPoints")
            parseTimingPoint(chart, line);
        else if (section == "Events")
            continue;  // background/video not needed here
        else
            parseKeyValue(chart, section, line);
    }
    return chart;
}

void limitSimultaneous(Parsed& p, int cap) {
    if (cap <= 0)
        return;
    std::vector<Ho> sorted = p.hitObjects;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Ho& a, const Ho& b) { return a.time < b.time; });
    const int keys = p.keyCount();
    std::vector<Ho> kept;
    std::vector<Ho> held;  // kept holds that are still down
    size_t i = 0;
    while (i < sorted.size()) {
        int t = sorted[i].time;
        size_t j = i + 1;
        while (j < sorted.size() && wrapSub(sorted[j].time, sorted[j - 1].time) <= CHORD_WINDOW_MS)
            j++;  // chained, so a slightly skewed chord of many notes stays together
        for (int h = static_cast<int>(held.size()) - 1; h >= 0; h--)
            if (held[h].endTime <= t)
                held.erase(held.begin() + h);  // released in time for this chord

        std::vector<Ho> group(sorted.begin() + i, sorted.begin() + j);
        int free = cap - static_cast<int>(held.size());
        std::vector<int> fingers;  // columns that already have a finger on them
        for (const Ho& h : held)
            fingers.push_back(h.lane(keys));
        while (free > 0 && !group.empty()) {
            int bestIdx = -1;
            int bestDist = -1;
            for (size_t c = 0; c < group.size(); c++) {
                int col = group[c].lane(keys);
                int dist = std::numeric_limits<int>::max();
                for (int f : fingers)
                    dist = std::min(dist, std::abs(col - f));
                if (fingers.empty())
                    dist = keys - 1 - col;  // nothing to stay away from: start with the leftmost note
                if (dist > bestDist) {
                    bestIdx = static_cast<int>(c);
                    bestDist = dist;
                }
            }
            Ho best = group[bestIdx];
            group.erase(group.begin() + bestIdx);
            kept.push_back(best);
            fingers.push_back(best.lane(keys));
            if (best.isHold())
                held.push_back(best);
            free--;
        }
        i = j;
    }
    p.hitObjects = kept;
}

double noteTimeMsToBeat(double timeMs, const std::vector<BpmSegment>& segments) {
    double beat = timeMsToBeat(timeMs, segments);
    double nearest = 0, nearestErr = std::numeric_limits<double>::max();
    bool found = false;
    for (int division : NOTE_SNAP_DIVISIONS) {
        for (int step = 0; step <= 1; step++) {
            double value = (std::floor(beat * division) + step) / division;
            double err = std::fabs(beatToTimeMs(value, segments) - timeMs);
            if (!found || err < nearestErr) {  // first minimum wins, like Python's min()
                nearest = value;
                nearestErr = err;
                found = true;
            }
        }
    }
    if (nearestErr <= NOTE_SNAP_TOLERANCE_MS + FLOAT_TOLERANCE)
        beat = nearest;
    return beat;
}

}  // namespace detail

// ---------------------------------------------------------------------------------------------
// convert
// ---------------------------------------------------------------------------------------------
std::optional<PjskChart> convert(std::istream& in, PjskChart::TailMode tailMode, bool tingedColumns, int minHoldMs,
                                 int maxSimultaneous) {
    Parsed p = detail::parse(in);
    if (p.mode != 3 || p.keyCount() <= 0 || p.hitObjects.size() <= 1)
        return std::nullopt;
    if (p.keyCount() > MAX_KEYS)
        throw std::length_error("OsuToPjsk: key count " + std::to_string(p.keyCount()) + " is too large");
    if (minHoldMs > 0) {
        for (Ho& ho : p.hitObjects)
            if (ho.isHold() && wrapSub(ho.endTime, ho.time) < minHoldMs)
                ho.hasEnd = false;  // plays as a tap
    }
    detail::limitSimultaneous(p, maxSimultaneous);
    return build(p, tailMode, tingedColumns);
}

std::optional<PjskChart> convertString(const std::string& osuText, PjskChart::TailMode tailMode, bool tingedColumns,
                                       int minHoldMs, int maxSimultaneous) {
    struct MemBuf : std::streambuf {
        explicit MemBuf(const std::string& s) {
            char* b = const_cast<char*>(s.data());
            setg(b, b, b + s.size());
        }
    } buf(osuText);
    std::istream in(&buf);
    return convert(in, tailMode, tingedColumns, minHoldMs, maxSimultaneous);
}

}  // namespace OsuToPjsk
}  // namespace fourresonance
}  // namespace opsu
