#pragma once
#include <vector>
#include "opsu/fourresonance/PjskGameplay.h"

namespace opsu {
namespace fourresonance {

/**
 * Autoplay for the PJSK mode: sends perfectly timed touches to a PjskGameplay. Port of
 * fourresonance/PjskAuto.java. Call update() every frame BEFORE PjskGameplay::update().
 *
 * Holds a non-owning reference: the gameplay must outlive this object.
 * Events are sorted by time; at equal times touches go down before they go up (zero-length holds),
 * remaining ties keep creation order (the Java comparator was a total order, so std::sort is fine).
 */
class PjskAuto {
public:
    explicit PjskAuto(PjskGameplay& gameplay);

    /** Sends every touch that is due at nowMs, each at its exact time. */
    void update(double nowMs);

private:
    /** First pointer id used (real fingers use small ids). */
    static constexpr int POINTER_BASE = 1000;

    PjskGameplay& gameplay;
    std::vector<double> times;
    std::vector<int> pointers;
    std::vector<double> lanes;
    std::vector<bool> down;
    size_t next = 0;
};

}  // namespace fourresonance
}  // namespace opsu
