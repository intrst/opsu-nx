#include "platform/input/AndroidInput.h"

#include <android/input.h>
#include <android/keycodes.h>
#include <android_native_app_glue.h>

namespace opsu {
namespace {

// ---------------------------------------------------------------------------------------------------------------------
// The ONLY code that reads platform events. For GameActivity (GameActivityMotionEvent / GameActivityKeyEvent) replace these two.
// ---------------------------------------------------------------------------------------------------------------------
bool readMotionEvent(const AInputEvent* ev, MotionSample& m) {
    const int32_t raw = AMotionEvent_getAction(ev);
    const int32_t action = raw & AMOTION_EVENT_ACTION_MASK;
    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:         m.kind = MotionSample::DOWN; break;
        case AMOTION_EVENT_ACTION_POINTER_DOWN: m.kind = MotionSample::DOWN; break;
        case AMOTION_EVENT_ACTION_UP:           m.kind = MotionSample::UP; break;
        case AMOTION_EVENT_ACTION_POINTER_UP:   m.kind = MotionSample::UP; break;
        case AMOTION_EVENT_ACTION_MOVE:         m.kind = MotionSample::MOVE; break;
        case AMOTION_EVENT_ACTION_CANCEL:       m.kind = MotionSample::CANCEL; break;
        case AMOTION_EVENT_ACTION_SCROLL:       m.kind = MotionSample::SCROLL; break;
        default:                                m.kind = MotionSample::OTHER; return true;  // hover, outside, button press ...
    }
    m.actionIndex = (raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    size_t n = AMotionEvent_getPointerCount(ev);
    if (n > static_cast<size_t>(MotionSample::MAX_POINTS)) n = MotionSample::MAX_POINTS;
    m.count = static_cast<int>(n);
    for (size_t i = 0; i < n; i++) {
        m.pts[i].id = AMotionEvent_getPointerId(ev, i);
        m.pts[i].x = AMotionEvent_getX(ev, i);
        m.pts[i].y = AMotionEvent_getY(ev, i);
    }
    if (m.kind == MotionSample::SCROLL && n > 0) {
        m.hscroll = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HSCROLL, 0);
        m.vscroll = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_VSCROLL, 0);
    }
    return true;
}

struct KeyPair { int android; int gdx; };

// AKEYCODE_* -> libGDX Input.Keys. libGDX copied the Android numbers (ESCAPE, END, INSERT and F1..F12 since 1.9.13), so the two
// columns are equal EXCEPT for Ctrl: libGDX Keys.CONTROL_LEFT/RIGHT are 129/130 but Android sends 113/114 (libGDX issue #7189).
// The table is explicit so that a key is only forwarded when the game knows it, and so all numbers are listed in one place.
const KeyPair kKeys[] = {
    {AKEYCODE_BACK, 4},                                                       // Keys.BACK == Input.ANDROID_BACK == Input.KEY_BACK
    {AKEYCODE_MENU, 82},                                                      // Keys.MENU == Input.ANDROID_MENU
    {AKEYCODE_ESCAPE, 111}, {AKEYCODE_ENTER, 66}, {AKEYCODE_NUMPAD_ENTER, 160},
    {AKEYCODE_SPACE, 62}, {AKEYCODE_TAB, 61},
    {AKEYCODE_DEL, 67},                                                       // Keys.DEL = backspace
    {AKEYCODE_FORWARD_DEL, 112},                                              // Keys.FORWARD_DEL (Input.KEY_DELETE is Keys.DEL = 67)
    {AKEYCODE_DPAD_UP, 19}, {AKEYCODE_DPAD_DOWN, 20}, {AKEYCODE_DPAD_LEFT, 21}, {AKEYCODE_DPAD_RIGHT, 22},
    {AKEYCODE_PAGE_UP, 92}, {AKEYCODE_PAGE_DOWN, 93}, {AKEYCODE_MOVE_END, 123},
    {AKEYCODE_0, 7}, {AKEYCODE_1, 8}, {AKEYCODE_2, 9}, {AKEYCODE_3, 10}, {AKEYCODE_4, 11},
    {AKEYCODE_5, 12}, {AKEYCODE_6, 13}, {AKEYCODE_7, 14}, {AKEYCODE_8, 15}, {AKEYCODE_9, 16},
    {AKEYCODE_A, 29}, {AKEYCODE_B, 30}, {AKEYCODE_C, 31}, {AKEYCODE_D, 32}, {AKEYCODE_E, 33}, {AKEYCODE_F, 34},
    {AKEYCODE_G, 35}, {AKEYCODE_H, 36}, {AKEYCODE_I, 37}, {AKEYCODE_J, 38}, {AKEYCODE_K, 39}, {AKEYCODE_L, 40},
    {AKEYCODE_M, 41}, {AKEYCODE_N, 42}, {AKEYCODE_O, 43}, {AKEYCODE_P, 44}, {AKEYCODE_Q, 45}, {AKEYCODE_R, 46},
    {AKEYCODE_S, 47}, {AKEYCODE_T, 48}, {AKEYCODE_U, 49}, {AKEYCODE_V, 50}, {AKEYCODE_W, 51}, {AKEYCODE_X, 52},
    {AKEYCODE_Y, 53}, {AKEYCODE_Z, 54},
    {AKEYCODE_F1, 131}, {AKEYCODE_F2, 132}, {AKEYCODE_F3, 133}, {AKEYCODE_F4, 134}, {AKEYCODE_F5, 135}, {AKEYCODE_F6, 136},
    {AKEYCODE_F7, 137}, {AKEYCODE_F8, 138}, {AKEYCODE_F9, 139}, {AKEYCODE_F10, 140}, {AKEYCODE_F11, 141}, {AKEYCODE_F12, 142},
    {AKEYCODE_SHIFT_LEFT, 59}, {AKEYCODE_SHIFT_RIGHT, 60}, {AKEYCODE_CTRL_LEFT, 129}, {AKEYCODE_CTRL_RIGHT, 130},  // 113 / 114 on Android
    {AKEYCODE_ALT_LEFT, 57}, {AKEYCODE_ALT_RIGHT, 58},
    {AKEYCODE_GRAVE, 68}, {AKEYCODE_MINUS, 69}, {AKEYCODE_EQUALS, 70}, {AKEYCODE_PLUS, 81},
    {AKEYCODE_COMMA, 55}, {AKEYCODE_PERIOD, 56}, {AKEYCODE_SLASH, 76}, {AKEYCODE_SEMICOLON, 74}, {AKEYCODE_APOSTROPHE, 75},
    {AKEYCODE_BACKSLASH, 73}, {AKEYCODE_LEFT_BRACKET, 71}, {AKEYCODE_RIGHT_BRACKET, 72},
    {AKEYCODE_NUMPAD_0, 144}, {AKEYCODE_NUMPAD_1, 145}, {AKEYCODE_NUMPAD_2, 146}, {AKEYCODE_NUMPAD_3, 147},
    {AKEYCODE_NUMPAD_4, 148}, {AKEYCODE_NUMPAD_5, 149}, {AKEYCODE_NUMPAD_6, 150}, {AKEYCODE_NUMPAD_7, 151},
    {AKEYCODE_NUMPAD_8, 152}, {AKEYCODE_NUMPAD_9, 153},
};

int signum(float v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

}  // namespace

// ---- configuration ----

bool AndroidInput::scaled() const {
    return surfaceW > 0 && surfaceH > 0 && gameW > 0 && gameH > 0 && (surfaceW != gameW || surfaceH != gameH);
}

int AndroidInput::toGameX(float x) const {
    return scaled() ? static_cast<int>(x * static_cast<float>(gameW) / static_cast<float>(surfaceW)) : static_cast<int>(x);
}

int AndroidInput::toGameY(float y) const {
    return scaled() ? static_cast<int>(y * static_cast<float>(gameH) / static_cast<float>(surfaceH)) : static_cast<int>(y);
}

void AndroidInput::setSurfaceSize(int w, int h) {
    if (w == surfaceW && h == surfaceH) return;
    cancelAll();  // rotation / resize: the stored positions are in the old geometry
    surfaceW = w;
    surfaceH = h;
}

void AndroidInput::setGameResolution(const Resolution& r) {
    if (r.getWidth() == gameW && r.getHeight() == gameH) return;
    cancelAll();
    gameW = r.getWidth();
    gameH = r.getHeight();
}

// ---- events ----

int32_t AndroidInput::onInputEvent(const AInputEvent* event) {
    if (event == nullptr) return 0;
    switch (AInputEvent_getType(event)) {
        case AINPUT_EVENT_TYPE_MOTION: {
            MotionSample m;
            if (!readMotionEvent(event, m)) return 0;
            return handleMotion(m);
        }
        case AINPUT_EVENT_TYPE_KEY: {
            const int32_t action = AKeyEvent_getAction(event);
            if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP) return 0;  // ACTION_MULTIPLE: not used
            // Repeats (AKeyEvent_getRepeatCount() > 0) are forwarded as further key downs, like libGDX (text fields need them).
            return handleKey(action == AKEY_EVENT_ACTION_DOWN, AKeyEvent_getKeyCode(event));
        }
        default:
            return 0;
    }
}

int32_t AndroidInput::onAppInputEvent(android_app* app, AInputEvent* event) {
    auto* self = app ? static_cast<AndroidInput*>(app->userData) : nullptr;
    return self ? self->onInputEvent(event) : 0;
}

void AndroidInput::down(const TouchPoint& p) {
    int old = slots.find(p.id);
    if (old >= 0) {  // a DOWN for a finger we think is still down: its UP was lost
        router.touchCancelled(lastX[old], lastY[old], old, input::Buttons::LEFT);
        slots.release(p.id);
    }
    int slot = slots.acquire(p.id);
    if (slot < 0) return;  // more than 20 fingers: ignored
    lastX[slot] = toGameX(p.x);
    lastY[slot] = toGameY(p.y);
    router.touchDown(lastX[slot], lastY[slot], slot, input::Buttons::LEFT);
}

void AndroidInput::up(const TouchPoint& p) {
    int slot = slots.release(p.id);
    if (slot < 0) return;  // unknown finger (was ignored on down)
    router.touchUp(toGameX(p.x), toGameY(p.y), slot, input::Buttons::LEFT);
}

int32_t AndroidInput::handleMotion(const MotionSample& m) {
    switch (m.kind) {
        case MotionSample::DOWN:
            if (m.actionIndex < 0 || m.actionIndex >= m.count) return 0;
            down(m.pts[m.actionIndex]);
            return 1;
        case MotionSample::UP:
            if (m.actionIndex < 0 || m.actionIndex >= m.count) return 0;
            up(m.pts[m.actionIndex]);
            return 1;
        case MotionSample::MOVE:
            // MOVE carries every pointer; only the ones that really moved become touchDragged.
            for (int i = 0; i < m.count; i++) {
                int slot = slots.find(m.pts[i].id);
                if (slot < 0) continue;
                int x = toGameX(m.pts[i].x), y = toGameY(m.pts[i].y);
                if (x == lastX[slot] && y == lastY[slot]) continue;
                lastX[slot] = x;
                lastY[slot] = y;
                router.touchDragged(x, y, slot);
            }
            return 1;
        case MotionSample::CANCEL:
            cancelAll();  // Android cancels all pointers at once; the router decides per finger what that means
            return 1;
        case MotionSample::SCROLL:
            router.scrolled(static_cast<float>(-signum(m.hscroll)), static_cast<float>(-signum(m.vscroll)));
            return 1;
        default:
            return 0;
    }
}

void AndroidInput::cancelAll() {
    for (int slot = PointerSlots::MAX - 1; slot >= 0; slot--) {
        int id = slots.idAt(slot);
        if (id < 0) continue;
        slots.release(id);
        router.touchCancelled(lastX[slot], lastY[slot], slot, input::Buttons::LEFT);
    }
}

int AndroidInput::mapKeycode(int androidKeycode) {
    for (const KeyPair& k : kKeys)
        if (k.android == androidKeycode) return k.gdx;
    return -1;
}

int32_t AndroidInput::handleKey(bool down, int androidKeycode) {
    int key = mapKeycode(androidKeycode);
    if (key < 0) return 0;  // not ours: leave it to the system (volume keys, home, ...)
    if (down) router.keyDown(key);
    else router.keyUp(key);
    return 1;  // includes BACK: consumed, so the activity is not finished by the system
}

}  // namespace opsu
