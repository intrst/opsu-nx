#pragma once
#include <array>
#include <cstdint>
#include <functional>

#include "opsu/input/GameplayInput.h"
#include "opsu/input/InputTarget.h"
#include "opsu/input/RawTouchListener.h"

namespace opsu {

/**
 * Touch / key / scroll router. Port of the input part of fluddokt.opsu.fake.StateBasedGame
 * (touchDown, touchUp, touchDragged, touchCancelled, scrolled, keyDown, keyUp, keyTyped, inputLocked).
 *
 * Two paths, chosen per finger when it goes down:
 *  - RAW: the current target is a RawTouchListener and isRawTouchActive() is true. The finger is owned by the raw path
 *    until it is lifted. Its move/up/cancel go to `rawTarget` (even if the state changed meanwhile) and are never emulated
 *    as mouse events.
 *  - MENU: single-cursor mouse emulation. Pointer 0 is the mouse, any pointer > 0 is a right click at the last position of
 *    pointer 0.
 *
 * `pointer` is the libGDX-style pointer index (0..19), NOT the Android pointerId: see PointerSlots and AndroidInput.
 * All handlers return the libGDX `InputProcessor` result (true = consumed). Like the Java they return false except when
 * input is locked (touchDown / keyDown / keyTyped / scrolled), where they return true.
 *
 * Seams (all injected): current target, clock in ms, inputLocked (Java: SkinSwitcher.isBusy()).
 *
 * Deviations from the Java
 *  - Every handler is wrapped in try/catch + Log::error. The Java only wraps touchDown and the three raw paths; the menu path
 *    of touchUp/touchDragged and the key/scroll handlers would let an exception escape (which in C++ must not cross the
 *    Android glue).
 *  - `Input.x/y` (static) and `Input.isRightButtonDown` of the fake Input are kept here: getMouseX/getMouseY/isRightButtonDown.
 *  - The Java global listener list (GInputListener, consumeEvent) is NOT ported yet, so `scrolled` only calls the target.
 *  - `mouseMoved(x, y)` (desktop only; never called for touch) is not ported.
 *  - `GameplayInput` is only used for MAX_POINTERS here; the raw listener (the Game state) owns the GameplayInput.
 */
class TouchRouter {
public:
    static constexpr int MAX_POINTERS = GameplayInput::MAX_POINTERS;

    using TargetProvider = std::function<InputTarget*()>;
    using Clock = std::function<int64_t()>;
    using LockPredicate = std::function<bool()>;

    /**
     * @param target  returns the current state (may return nullptr: then menu events are dropped, raw events still work)
     * @param clock   milliseconds, monotonic (Java: TimeUtils.millis()); default: steady_clock
     * @param locked  true while a skin swap is running (Java: SkinSwitcher.isBusy()); default: never locked
     */
    explicit TouchRouter(TargetProvider target, Clock clock = nullptr, LockPredicate locked = nullptr);
    TouchRouter(const TouchRouter&) = delete;
    TouchRouter& operator=(const TouchRouter&) = delete;

    bool touchDown(int x, int y, int pointer, int button);
    bool touchUp(int x, int y, int pointer, int button);
    bool touchDragged(int x, int y, int pointer);
    /** One finger cancelled: RAW -> rawTouchCancelled for that finger only; MENU -> behaves like touchUp. */
    bool touchCancelled(int x, int y, int pointer, int button);
    /** Only vertical scrolling is used: target->mouseWheelMoved((int) -amountY). */
    bool scrolled(float amountX, float amountY);
    bool keyDown(int keycode);
    bool keyUp(int keycode);
    /** Java: only feeds the global listeners (not ported), so this only reports the lock. */
    bool keyTyped(char character);

    /** Call when the active state changes (Java enterNextState: touchX = touchY = 0). */
    void onStateEntered() { touchX = touchY = 0; }

    /** Last mouse position (Java `Input.x/y`): set by mousePressed and by dragging pointer 0. */
    int getMouseX() const { return mouseX; }
    int getMouseY() const { return mouseY; }
    /** Java `Input.isRightButtonDown`: true while a finger > 0 emulates the right button. */
    bool isRightButtonDown() const { return rightButtonDown; }

    /** Who owns a pointer right now (for tests / debugging). */
    enum Owner { OWNER_NONE = 0, OWNER_MENU = 1, OWNER_RAW = 2 };
    Owner getOwner(int pointer) const { return validPointer(pointer) ? static_cast<Owner>(pointerOwner[pointer]) : OWNER_NONE; }
    /** The listener that received the last raw DOWN (Java `rawTarget`; one field, shared by all raw fingers, like the Java). */
    RawTouchListener* getRawTarget() const { return rawTarget; }

    /**
     * Call before a RawTouchListener is destroyed: the router forgets it (the Java relied on the GC). Fingers owned by the
     * raw path stay owned (so their up/cancel is swallowed rather than emulated as mouse events); no callbacks are made.
     */
    void forgetRawTarget(RawTouchListener* l) { if (rawTarget == l) rawTarget = nullptr; }

private:
    static bool validPointer(int p) { return p >= 0 && p < MAX_POINTERS; }
    bool inputLocked() const { return locked && locked(); }
    RawTouchListener* getRawListener();

    // the Java private methods of the same names (global listeners left out)
    void mousePressed(int button, int x, int y);
    void mouseReleased(int button, int x, int y);
    void mouseClicked(int button, int x, int y, int clickCount);
    void mouseDragged(int oldx, int oldy, int newx, int newy);

    bool touchUpImpl(int x, int y, int pointer, int button);

    TargetProvider target;
    Clock clock;
    LockPredicate locked;

    int pointerOwner[MAX_POINTERS] = {};
    RawTouchListener* rawTarget = nullptr;

    bool rightIsPressed = false;
    bool rightButtonDown = false;
    int touchX = 0, touchY = 0;
    int64_t touchTime = 0;
    int oldx = 0, oldy = 0;
    int mouseX = 0, mouseY = 0;
};

/**
 * Maps Android pointerIds to router pointer slots like libGDX does: each active id gets the LOWEST FREE slot 0..19, the slot
 * is freed on up/cancel. This matters because slot 0 means "the mouse" and slot > 0 means "right click".
 * Pure (no Android headers) so it is tested on the desktop. The 21st simultaneous finger gets no slot (-1) and is ignored.
 */
class PointerSlots {
public:
    static constexpr int MAX = TouchRouter::MAX_POINTERS;

    /** Slot of an already active id, or -1. */
    int find(int id) const;
    /** Slot of the id; assigns the lowest free slot if it is new. Returns -1 if all 20 slots are used. */
    int acquire(int id);
    /** Frees the slot of the id and returns it, or -1 if the id was not active. */
    int release(int id);
    void clear();
    int activeCount() const;
    /** Id in a slot, or -1 if the slot is free. */
    int idAt(int slot) const { return slot >= 0 && slot < MAX ? ids[slot] : -1; }

private:
    // Android pointer ids are >= 0, so -1 marks a free slot.
    std::array<int, MAX> ids = [] { std::array<int, MAX> a{}; a.fill(-1); return a; }();
};

}  // namespace opsu
