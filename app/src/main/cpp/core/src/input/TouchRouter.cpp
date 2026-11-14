#include "opsu/input/TouchRouter.h"

#include <chrono>
#include <climits>
#include <cmath>
#include <exception>

#include "opsu/logging/Log.h"

namespace opsu {
namespace {

int64_t steadyMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

/** Java `(int) f`: truncates toward zero, NaN -> 0, out of range saturates. */
int javaFloatToInt(float f) {
    if (std::isnan(f)) return 0;
    if (f >= 2147483648.0f) return INT_MAX;
    if (f <= -2147483648.0f) return INT_MIN;
    return static_cast<int>(f);
}

/** Runs a handler body; an exception is logged (like Opsu.error in the Java) and never escapes. */
template <class F>
void guarded(const char* what, F&& body) {
    try {
        body();
    } catch (const std::exception& e) {
        Log::error("TouchRouter", what, e);
    } catch (...) {
        Log::error("TouchRouter", std::string(what) + ": unknown exception");
    }
}

/** Runs a function when the scope ends (also when a target throws), so router state cannot get stuck. */
template <class F>
struct ScopeExit {
    F f;
    explicit ScopeExit(F fn) : f(std::move(fn)) {}
    ScopeExit(const ScopeExit&) = delete;
    ~ScopeExit() { f(); }
};
template <class F>
ScopeExit<F> onExit(F f) { return ScopeExit<F>(std::move(f)); }

}  // namespace

TouchRouter::TouchRouter(TargetProvider target, Clock clock, LockPredicate locked)
    : target(std::move(target)), clock(clock ? std::move(clock) : Clock(steadyMillis)), locked(std::move(locked)) {}

RawTouchListener* TouchRouter::getRawListener() {
    InputTarget* t = target ? target() : nullptr;
    if (auto* l = dynamic_cast<RawTouchListener*>(t)) {
        if (l->isRawTouchActive()) return l;
    }
    return nullptr;
}

// ---- the Java private mouse helpers (global GInputListener list not ported) ----

void TouchRouter::mousePressed(int button, int x, int y) {
    mouseX = x;  // Input.x / Input.y
    mouseY = y;
    if (InputTarget* t = target ? target() : nullptr) t->mousePressed(button, x, y);
}

void TouchRouter::mouseReleased(int button, int x, int y) {
    if (InputTarget* t = target ? target() : nullptr) t->mouseReleased(button, x, y);
}

void TouchRouter::mouseClicked(int button, int x, int y, int clickCount) {
    if (InputTarget* t = target ? target() : nullptr) t->mouseClicked(button, x, y, clickCount);
}

void TouchRouter::mouseDragged(int ox, int oy, int nx, int ny) {
    if (InputTarget* t = target ? target() : nullptr) t->mouseDragged(ox, oy, nx, ny);
}

// ---- touch ----

bool TouchRouter::touchDown(int x, int y, int pointer, int button) {
    if (inputLocked()) return true;  // skin swap in progress
    guarded("touchDown", [&] {
        // Gameplay mode: every finger is independent. Nothing is emulated as a mouse.
        RawTouchListener* raw = getRawListener();
        if (raw != nullptr && validPointer(pointer)) {
            pointerOwner[pointer] = OWNER_RAW;
            rawTarget = raw;
            raw->rawTouchDown(pointer, x, y);
            return;
        }
        if (validPointer(pointer)) pointerOwner[pointer] = OWNER_MENU;

        // Menu mode: single-cursor mouse emulation.
        if (pointer > 0) {
            if (rightIsPressed) mouseReleased(input::Buttons::RIGHT, oldx, oldy);
            mousePressed(input::Buttons::RIGHT, oldx, oldy);
            rightButtonDown = true;  // container.getInput().setMouseRighButtontDown(true)
            rightIsPressed = true;
            touchX = oldx;
            touchY = oldy;
            touchTime = clock();
        } else {
            mousePressed(button, x, y);
            oldx = x;
            oldy = y;
            touchX = x;
            touchY = y;
            touchTime = clock();
        }
    });
    return false;
}

bool TouchRouter::touchUp(int x, int y, int pointer, int button) {
    // NOTE: like the Java, touchUp is NOT blocked by inputLocked(): a finger that went down before the lock must still be
    // released, otherwise it would stay "owned" forever.
    guarded("touchUp", [&] { touchUpImpl(x, y, pointer, button); });
    return false;
}

bool TouchRouter::touchUpImpl(int x, int y, int pointer, int button) {
    if (validPointer(pointer)) {
        bool wasRaw = pointerOwner[pointer] == OWNER_RAW;
        pointerOwner[pointer] = OWNER_NONE;
        if (wasRaw) {
            if (rawTarget != nullptr) rawTarget->rawTouchUp(pointer, x, y);
            return false;
        }
    }

    if (pointer > 0) {
        // state is reset even if a target throws (the Java would have left it stuck)
        auto reset = onExit([&] {
            rightButtonDown = false;  // setMouseRighButtontDown(false)
            rightIsPressed = false;
        });
        int dx = oldx - touchX;
        int dy = oldy - touchY;
        if (clock() - touchTime < 500 && dx * dx + dy * dy < 10 * 10) mouseClicked(input::Buttons::RIGHT, oldx, oldy, 1);
        mouseReleased(input::Buttons::RIGHT, oldx, oldy);
    } else {
        auto reset = onExit([&] {
            oldx = x;
            oldy = y;
        });
        int dx = x - touchX;
        int dy = y - touchY;
        if (clock() - touchTime < 500 && dx * dx + dy * dy < 10 * 10) mouseClicked(button, x, y, 1);
        mouseReleased(button, x, y);
    }
    return false;
}

bool TouchRouter::touchCancelled(int x, int y, int pointer, int button) {
    // Only the cancelled finger is released (releasing every finger made the others vanish).
    if (validPointer(pointer) && pointerOwner[pointer] == OWNER_RAW) {
        pointerOwner[pointer] = OWNER_NONE;  // rawTarget itself stays: other raw fingers still use it
        guarded("rawTouchCancelled", [&] {
            if (rawTarget != nullptr) rawTarget->rawTouchCancelled(pointer);
        });
        return false;
    }
    return touchUp(x, y, pointer, button);
}

bool TouchRouter::touchDragged(int x, int y, int pointer) {
    if (validPointer(pointer) && pointerOwner[pointer] == OWNER_RAW) {
        guarded("rawTouchMoved", [&] {
            if (rawTarget != nullptr) rawTarget->rawTouchMoved(pointer, x, y);
        });
        return false;
    }

    guarded("touchDragged", [&] {
        if (pointer == 0) {  // only the "mouse" finger drags
            mouseX = x;      // Input.x / Input.y
            mouseY = y;
            auto reset = onExit([&] {
                oldx = x;
                oldy = y;
            });
            mouseDragged(oldx, oldy, x, y);
        }
    });
    return false;
}

// ---- scroll and keys ----

bool TouchRouter::scrolled(float /*amountX*/, float amountY) {
    if (inputLocked()) return true;  // skin swap in progress
    guarded("scrolled", [&] {
        // Java: currentState.mouseWheelMoved((int) -amountY); the global listeners (not ported) get that * 120.
        if (InputTarget* t = target ? target() : nullptr) t->mouseWheelMoved(javaFloatToInt(-amountY));
    });
    return false;
}

bool TouchRouter::keyDown(int keycode) {
    if (inputLocked()) return true;  // skin swap in progress
    guarded("keyDown", [&] {
        if (InputTarget* t = target ? target() : nullptr) t->keyPressed(keycode, '\0');
    });
    return false;
}

bool TouchRouter::keyUp(int keycode) {
    // Java: no lock check on key up either (a key pressed before the lock must still be released).
    guarded("keyUp", [&] {
        if (InputTarget* t = target ? target() : nullptr) t->keyReleased(keycode, '\0');
    });
    return false;
}

bool TouchRouter::keyTyped(char /*character*/) {
    return inputLocked();
}

// ---- PointerSlots ----

int PointerSlots::find(int id) const {
    for (int i = 0; i < MAX; i++)
        if (ids[i] == id) return i;
    return -1;
}

int PointerSlots::acquire(int id) {
    if (id < 0) return -1;
    int s = find(id);
    if (s >= 0) return s;
    for (int i = 0; i < MAX; i++) {
        if (ids[i] < 0) {
            ids[i] = id;
            return i;
        }
    }
    return -1;
}

int PointerSlots::release(int id) {
    int s = find(id);
    if (s >= 0) ids[s] = -1;
    return s;
}

void PointerSlots::clear() { ids.fill(-1); }

int PointerSlots::activeCount() const {
    int n = 0;
    for (int v : ids)
        if (v >= 0) n++;
    return n;
}

}  // namespace opsu
