#include "opsu/fourresonance/PjskNote.h"

namespace opsu {
namespace fourresonance {

PjskNote::PjskNote(const PjskChart::Note& note)
    : data(&note),
      headMs(static_cast<double>(note.timeMs)),
      tailMs(static_cast<double>(note.endTimeMs)),
      // Java: `data.lane - data.size` is float arithmetic, widened to double afterwards.
      laneL(static_cast<double>(note.lane - note.size)),
      laneR(static_cast<double>(note.lane + note.size)) {}

}  // namespace fourresonance
}  // namespace opsu
