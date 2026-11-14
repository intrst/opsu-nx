#pragma once

namespace opsu {
namespace input {

/**
 * Mouse button numbers, same as libGDX `Input.Buttons` (fake `Input.MOUSE_*_BUTTON` in the Java).
 * Note: these are NOT the Slick/LWJGL numbers (there RIGHT was 1 as well, but MIDDLE was 2 and there was no BACK).
 */
namespace Buttons {
inline constexpr int LEFT = 0, RIGHT = 1, MIDDLE = 2, BACK = 3, FORWARD = 4;
}  // namespace Buttons

}  // namespace input

/**
 * The menu-side input callbacks of a game state: the mouse and key methods of the Java `BasicGameState` that
 * `fluddokt.opsu.fake.StateBasedGame` calls (`mousePressed`, `mouseReleased`, `mouseClicked`, `mouseDragged`,
 * `mouseMoved`, `mouseWheelMoved`, `keyPressed`, `keyReleased`).
 *
 * Every method has an empty default body (like `BasicGameState`), so a state only overrides what it uses.
 * A state that also wants raw multitouch (gameplay) additionally implements `RawTouchListener`; the router finds that
 * with `dynamic_cast`, so this class must stay polymorphic and the build must keep RTTI on (the NDK default).
 *
 * Coordinates are game-resolution pixels, origin top-left, y down.
 */
class InputTarget {
public:
    virtual ~InputTarget() = default;

    virtual void mousePressed(int /*button*/, int /*x*/, int /*y*/) {}
    virtual void mouseReleased(int /*button*/, int /*x*/, int /*y*/) {}
    /** A press and release close together (see TouchRouter: < 500 ms and < 10 px). Called BEFORE mouseReleased. */
    virtual void mouseClicked(int /*button*/, int /*x*/, int /*y*/, int /*clickCount*/) {}
    virtual void mouseDragged(int /*oldx*/, int /*oldy*/, int /*newx*/, int /*newy*/) {}
    virtual void mouseMoved(int /*oldx*/, int /*oldy*/, int /*newx*/, int /*newy*/) {}
    /** Java: `mouseWheelMoved(int newValue)`. Positive = scroll up (away from the user). */
    virtual void mouseWheelMoved(int /*amount*/) {}
    /** `c` is always 0 when the event comes from StateBasedGame.keyDown (the Java passes `(char) 0`). */
    virtual void keyPressed(int /*key*/, char /*c*/) {}
    virtual void keyReleased(int /*key*/, char /*c*/) {}
};

}  // namespace opsu
