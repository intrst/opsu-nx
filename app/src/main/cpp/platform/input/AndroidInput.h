#pragma once
#include <cstdint>

#include "opsu/Resolution.h"
#include "opsu/input/TouchRouter.h"

// Forward declarations only: the NDK headers are included by AndroidInput.cpp (the glue is easy to swap for GameActivity).
struct AInputEvent;
struct android_app;

namespace opsu {

/** One pointer of a motion event (already read out of the platform event). */
struct TouchPoint {
    int id = 0;      // Android pointerId (NOT the router's pointer number)
    float x = 0, y = 0;  // surface pixels, origin top-left
};

/**
 * A platform-independent copy of an Android motion event. Only `readMotionEvent()` in AndroidInput.cpp knows how to fill it
 * from an `AInputEvent`; for GameActivity only that function has to be replaced.
 */
struct MotionSample {
    static constexpr int MAX_POINTS = 32;
    enum Kind { DOWN, UP, MOVE, CANCEL, SCROLL, OTHER };
    Kind kind = OTHER;
    int actionIndex = 0;  // the pointer that went down / up (POINTER_DOWN / POINTER_UP), 0 for DOWN / UP
    int count = 0;
    TouchPoint pts[MAX_POINTS];
    float hscroll = 0, vscroll = 0;  // AXIS_HSCROLL / AXIS_VSCROLL (SCROLL only)
};

/**
 * Android glue for the TouchRouter: turns AInputEvents (motion + key) into router calls.
 *
 *  - Pointer slots, like libGDX: every active Android pointerId gets the lowest free slot 0..19 (PointerSlots); the slot is the
 *    router's `pointer`. The slot is freed on up / cancel.
 *  - DOWN / POINTER_DOWN -> touchDown(x, y, slot, LEFT); MOVE -> touchDragged for each pointer whose (game) position changed;
 *    UP / POINTER_UP -> touchUp; CANCEL -> touchCancelled for EVERY active pointer (highest slot first, so a menu "right
 *    button" is released before the "mouse"). A DOWN for an id that is already active (a missed UP) first cancels the old slot.
 *  - Mouse wheel (ACTION_SCROLL) -> scrolled(-sign(hscroll), -sign(vscroll)) like libGDX.
 *  - Keys: AKEYCODE_* -> libGDX key numbers (mapKeycode); unmapped keys return 0 so the system keeps them (volume, home, ...).
 *    BACK is mapped to libGDX Keys.BACK (4 == Input.ANDROID_BACK == Input.KEY_BACK in the Java) and consumed (returns 1).
 *  - Coordinates: Android gives pixels with the origin top-left, like libGDX, so there is no flip. The Java game size equals the
 *    surface size (AppGameContainer: width = Gdx.graphics.getWidth()), so by default pixels pass through unchanged (cast to
 *    int, like libGDX). If setGameResolution() is given a size that differs from setSurfaceSize(), x and y are scaled.
 *  - If the surface size or game resolution changes while fingers are down (rotation), all fingers are cancelled.
 *
 * Not thread-safe: call everything from the thread that runs the router (the native_app_glue main thread).
 * Return values follow AInputEvent handlers: 1 = handled, 0 = not handled.
 */
class AndroidInput {
public:
    explicit AndroidInput(TouchRouter& router) : router(router) {}
    AndroidInput(const AndroidInput&) = delete;
    AndroidInput& operator=(const AndroidInput&) = delete;

    /** Size of the Android surface in pixels (0 = unknown: pass coordinates through). */
    void setSurfaceSize(int w, int h);
    /** Size the game renders at. Never set (or equal to the surface size) = no scaling. */
    void setGameResolution(const Resolution& r);

    /** Handles one platform event (motion or key). */
    int32_t onInputEvent(const AInputEvent* event);
    /** native_app_glue hook: `app->userData = &androidInput; app->onInputEvent = &AndroidInput::onAppInputEvent;` */
    static int32_t onAppInputEvent(android_app* app, AInputEvent* event);

    /** The platform-independent halves (these are what the tests drive directly). */
    int32_t handleMotion(const MotionSample& m);
    int32_t handleKey(bool down, int androidKeycode);

    /**
     * Cancels every active finger (router.touchCancelled) and frees all slots. Call it when the app loses focus, the window is
     * destroyed or the activity pauses, so no finger stays stuck.
     */
    void cancelAll();

    int activePointers() const { return slots.activeCount(); }
    /** Slot of an Android pointerId, or -1. */
    int slotOf(int androidPointerId) const { return slots.find(androidPointerId); }

    /** AKEYCODE_* -> libGDX key number, or -1 if the game does not use the key. */
    static int mapKeycode(int androidKeycode);

private:
    int toGameX(float x) const;
    int toGameY(float y) const;
    bool scaled() const;
    void down(const TouchPoint& p);
    void up(const TouchPoint& p);

    TouchRouter& router;
    PointerSlots slots;
    int lastX[PointerSlots::MAX] = {};
    int lastY[PointerSlots::MAX] = {};
    int surfaceW = 0, surfaceH = 0, gameW = 0, gameH = 0;
};

}  // namespace opsu
