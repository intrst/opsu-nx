#pragma once
#include <cstdint>

namespace opsu {

/** Blend factors the draw modes need (a subset of what GL/Vulkan offer; the backend maps them 1:1). */
enum class BlendFactor : uint8_t { Zero, One, SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha };

/** Colour write mask + blend function of one draw mode. Non-premultiplied alpha, as in the Java. */
struct DrawModeState {
    bool maskR = true, maskG = true, maskB = true, maskA = true;
    BlendFactor src = BlendFactor::SrcAlpha;
    BlendFactor dst = BlendFactor::OneMinusSrcAlpha;

    bool operator==(const DrawModeState& o) const {
        return maskR == o.maskR && maskG == o.maskG && maskB == o.maskB && maskA == o.maskA && src == o.src &&
               dst == o.dst;
    }
    bool operator!=(const DrawModeState& o) const { return !(*this == o); }
};

/** Numbers of fluddokt.opsu.fake.Graphics.MODE_*. */
namespace drawmode {
constexpr int NORMAL = 1;       ///< RGBA, SRC_ALPHA / ONE_MINUS_SRC_ALPHA
constexpr int ALPHA_MAP = 2;    ///< mask B+A only, ONE / ZERO (slider body: writes the alpha map)
constexpr int ALPHA_BLEND = 3;  ///< mask R+G only, DST_ALPHA / ONE_MINUS_DST_ALPHA
constexpr int ADD = 4;          ///< RGBA, SRC_ALPHA / ONE (additive)
}  // namespace drawmode

bool isValidDrawMode(int mode);

/** Mask and blend of a mode (port of Graphics.setDrawMode). Throws std::invalid_argument("Unknown Draw Mode"). */
DrawModeState drawModeState(int mode);

}  // namespace opsu
