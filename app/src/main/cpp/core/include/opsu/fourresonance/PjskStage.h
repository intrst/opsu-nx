#pragma once

namespace opsu {
namespace fourresonance {

/**
 * PARTIAL port of fourresonance/PjskStage.java (rendering/layout, 9 KB). Only the pure constants
 * that PjskGameplay needs are here; the stage drawing and touchToLane() wait for the backend.
 */
namespace PjskStage {

/** Extra width (lane units) the touch hitbox of a note has on each side (Sekai: leniency 1.0). */
inline constexpr double HITBOX_LENIENCY = 1.0;
/** Half-height of the touch hitbox around the judgment line, in lane widths (Sekai: 5.0). */
inline constexpr double HITBOX_VERTICAL_HALF_LANES = 5.0;

}  // namespace PjskStage

}  // namespace fourresonance
}  // namespace opsu
