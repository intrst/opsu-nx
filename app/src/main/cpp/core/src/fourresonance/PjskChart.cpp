#include "opsu/fourresonance/PjskChart.h"
#include "opsu/fourresonance/OsuToPjsk.h"

namespace opsu {
namespace fourresonance {

double PjskChart::timeToBeat(double timeMs) const { return OsuToPjsk::timeMsToBeat(timeMs, bpmSegments); }

double PjskChart::beatToTime(double beat) const { return OsuToPjsk::beatToTimeMs(beat, bpmSegments); }

double PjskChart::scaledTime(double timeMs) const {
    const int n = static_cast<int>(timeScale.size());
    if (n == 0) return timeMs;
    if (scaledAtChange.empty()) {
        scaledAtChange.assign(static_cast<size_t>(n), 0.0);
        for (int i = 1; i < n; i++) {
            const TimeScaleChange& prev = timeScale[i - 1];
            const TimeScaleChange& cur = timeScale[i];
            scaledAtChange[i] = scaledAtChange[i - 1] + (cur.timeMs - prev.timeMs) * prev.timeScale;
        }
    }
    const TimeScaleChange& first = timeScale[0];
    if (timeMs <= first.timeMs) return (timeMs - first.timeMs) * first.timeScale;
    // binary search for the last change at or before timeMs
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) >> 1;
        if (timeScale[mid].timeMs <= timeMs) lo = mid;
        else hi = mid - 1;
    }
    const TimeScaleChange& c = timeScale[lo];
    return scaledAtChange.at(static_cast<size_t>(lo)) + (timeMs - c.timeMs) * c.timeScale;
}

}  // namespace fourresonance
}  // namespace opsu
