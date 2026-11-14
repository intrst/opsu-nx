#pragma once

namespace opsu {

/**
 * Implemented by a game state that wants raw, independent per-finger touch events
 * instead of the single-cursor mouse emulation used by the menus.
 *
 * While isRawTouchActive() returns true at the moment a finger goes down, that finger is
 * "owned" by the raw path until it is lifted: its move/up events are never turned into
 * emulated mouse events, even if the active state changes meanwhile.
 */
class RawTouchListener {
public:
    virtual ~RawTouchListener() = default;

    /** Whether this state wants raw multitouch right now (checked on every finger-down). */
    virtual bool isRawTouchActive() = 0;
    /** A finger went down. */
    virtual void rawTouchDown(int pointer, int x, int y) = 0;
    /** A finger moved. */
    virtual void rawTouchMoved(int pointer, int x, int y) = 0;
    /** A finger was lifted. */
    virtual void rawTouchUp(int pointer, int x, int y) = 0;
    /** The system cancelled this one finger. Other fingers are NOT affected. */
    virtual void rawTouchCancelled(int pointer) = 0;
};

}  // namespace opsu
