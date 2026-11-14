#include "opsu/fourresonance/PjskAuto.h"
#include <algorithm>
#include <numeric>
#include "opsu/util/JavaMath.h"

namespace opsu {
namespace fourresonance {

PjskAuto::PjskAuto(PjskGameplay& gp) : gameplay(gp) {
    const std::vector<PjskNote>& notes = gameplay.getNotes();
    size_t n = 0;
    for (const PjskNote& note : notes) n += note.isHold() ? 2 : 1;

    std::vector<double> t(n), l(n);
    std::vector<bool> d(n);
    std::vector<int> p(n);
    size_t k = 0;
    for (size_t i = 0; i < notes.size(); i++) {
        const PjskNote& note = notes[i];
        const int ptr = POINTER_BASE + static_cast<int>(i);
        t[k] = note.headMs; d[k] = true; p[k] = ptr; l[k] = note.data->lane; k++;
        if (note.isHold()) {
            t[k] = note.tailMs; d[k] = false; p[k] = ptr; l[k] = note.data->lane; k++;
        }
    }
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), size_t{0});
    // by time; at equal times touches go down before they go up (zero-length holds)
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) -> bool {
        int c = java::compare(t[a], t[b]);
        if (c != 0) return c < 0;
        if (d[a] == d[b]) return a < b;
        return static_cast<bool>(d[a]);
    });
    times.resize(n); pointers.resize(n); lanes.resize(n); down.resize(n);
    for (size_t i = 0; i < n; i++) {
        times[i] = t[order[i]]; pointers[i] = p[order[i]]; lanes[i] = l[order[i]]; down[i] = d[order[i]];
    }
}

void PjskAuto::update(double nowMs) {
    while (next < times.size() && times[next] <= nowMs) {
        if (down[next]) gameplay.touchDown(pointers[next], lanes[next], times[next]);
        else gameplay.touchUp(pointers[next], times[next]);
        next++;
    }
}

}  // namespace fourresonance
}  // namespace opsu
