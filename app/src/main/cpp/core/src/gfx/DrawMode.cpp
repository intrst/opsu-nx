#include "opsu/gfx/DrawMode.h"

#include <stdexcept>

namespace opsu {

bool isValidDrawMode(int mode) { return mode >= drawmode::NORMAL && mode <= drawmode::ADD; }

DrawModeState drawModeState(int mode) {
    DrawModeState s;
    switch (mode) {
        case drawmode::NORMAL:
            break;  // defaults
        case drawmode::ALPHA_MAP:
            s.maskR = false; s.maskG = false; s.maskB = true; s.maskA = true;
            s.src = BlendFactor::One; s.dst = BlendFactor::Zero;
            break;
        case drawmode::ALPHA_BLEND:
            s.maskR = true; s.maskG = true; s.maskB = false; s.maskA = false;
            s.src = BlendFactor::DstAlpha; s.dst = BlendFactor::OneMinusDstAlpha;
            break;
        case drawmode::ADD:
            s.src = BlendFactor::SrcAlpha; s.dst = BlendFactor::One;
            break;
        default:
            throw std::invalid_argument("Unknown Draw Mode");
    }
    return s;
}

}  // namespace opsu
