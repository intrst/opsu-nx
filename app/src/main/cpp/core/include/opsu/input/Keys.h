#pragma once

namespace opsu {
namespace input {
/**
 * Letter key codes using libGDX/Android numbering (Input.Keys.A == 29 ...), as used by
 * GameMod's shortcut keys. Remap here once the windowing backend (SDL2, ...) is chosen.
 */
namespace Keys {
inline constexpr int A = 29, B = 30, C = 31, D = 32, E = 33, F = 34, G = 35, H = 36, I = 37, J = 38,
                     K = 39, L = 40, M = 41, N = 42, O = 43, P = 44, Q = 45, R = 46, S = 47, T = 48,
                     U = 49, V = 50, W = 51, X = 52, Y = 53, Z = 54;
}  // namespace Keys
}  // namespace input
}  // namespace opsu
