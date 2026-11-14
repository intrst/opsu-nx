// T4 tests: TouchRouter (port of the touch part of fluddokt.opsu.fake.StateBasedGame) and PointerSlots.
// The Java cannot be run here, so there is no golden file: every expected event sequence below was written by hand from
// StateBasedGame.java (touchDown / touchUp / touchCancelled / touchDragged / scrolled), the reasoning is in the comments.
// Time is a fake clock, so the 500 ms rule is exact and the test is instant.
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include "opsu/input/GameplayInput.h"
#include "opsu/input/InputTarget.h"
#include "opsu/input/TouchRouter.h"

using namespace opsu;
using input::Buttons::LEFT;
using input::Buttons::RIGHT;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define CHECK_LOG(rec, ...) do { std::vector<std::string> want = {__VA_ARGS__}; if ((rec).log != want) { \
    std::printf("FAIL %s:%d  event log differs\n  got: ", __FILE__, __LINE__); for (auto& s : (rec).log) std::printf("[%s] ", s.c_str()); \
    std::printf("\n  want:"); for (auto& s : want) std::printf(" [%s]", s.c_str()); std::printf("\n"); failures++; } } while (0)

static int64_t g_now = 5000;
static void advance(int ms) { g_now += ms; }

static std::string fmt(const char* name, int a, int b = 0, int c = 0, int d = 0) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s(%d,%d,%d,%d)", name, a, b, c, d);
    return buf;
}

// A menu state that records every callback as a string.
struct Rec : InputTarget {
    std::vector<std::string> log;
    bool throwOnPress = false, throwOnRelease = false, throwOnClick = false;
    void mousePressed(int b, int x, int y) override { log.push_back(fmt("press", b, x, y)); if (throwOnPress) throw std::runtime_error("boom"); }
    void mouseReleased(int b, int x, int y) override { log.push_back(fmt("release", b, x, y)); if (throwOnRelease) throw std::runtime_error("boom"); }
    void mouseClicked(int b, int x, int y, int n) override { log.push_back(fmt("click", b, x, y, n)); if (throwOnClick) throw std::runtime_error("boom"); }
    void mouseDragged(int ox, int oy, int nx, int ny) override { log.push_back(fmt("drag", ox, oy, nx, ny)); }
    void mouseWheelMoved(int a) override { log.push_back(fmt("wheel", a)); }
    void keyPressed(int k, char c) override { log.push_back(fmt("keyDown", k, c)); }
    void keyReleased(int k, char c) override { log.push_back(fmt("keyUp", k, c)); }
};

// A gameplay state: menu callbacks (recorded in the base) plus raw touch.
struct RawRec : Rec, RawTouchListener {
    bool active = true;
    std::vector<std::string> raw;
    bool isRawTouchActive() override { return active; }
    void rawTouchDown(int p, int x, int y) override { raw.push_back(fmt("rawDown", p, x, y)); }
    void rawTouchMoved(int p, int x, int y) override { raw.push_back(fmt("rawMove", p, x, y)); }
    void rawTouchUp(int p, int x, int y) override { raw.push_back(fmt("rawUp", p, x, y)); }
    void rawTouchCancelled(int p) override { raw.push_back(fmt("rawCancel", p)); }
};
#define CHECK_RAW(rec, ...) do { std::vector<std::string> want = {__VA_ARGS__}; if ((rec).raw != want) { \
    std::printf("FAIL %s:%d  raw log differs\n  got: ", __FILE__, __LINE__); for (auto& s : (rec).raw) std::printf("[%s] ", s.c_str()); \
    std::printf("\n  want:"); for (auto& s : want) std::printf(" [%s]", s.c_str()); std::printf("\n"); failures++; } } while (0)

struct Fixture {
    InputTarget* current = nullptr;
    bool locked = false;
    TouchRouter router;
    Fixture() : router([this] { return current; }, [] { return g_now; }, [this] { return locked; }) {}
};

// ---------------------------------------------------------------------------------------------------------------------
// 1. single tap -> pressed, clicked, released (in that order); a slow tap or a long move -> no click
// Java touchUp, pointer 0: dx,dy from touchX/touchY (set on down); click if timeSince(touchTime) < 500 && dx*dx+dy*dy < 100;
// mouseClicked is called BEFORE mouseReleased.
// ---------------------------------------------------------------------------------------------------------------------
static void testTap() {
    Rec r; Fixture f; f.current = &r;
    f.router.touchDown(100, 200, 0, LEFT);
    advance(120);
    f.router.touchUp(103, 204, 0, LEFT);  // moved 3,4 -> 25 < 100
    CHECK_LOG(r, "press(0,100,200,0)", "click(0,103,204,1)", "release(0,103,204,0)");
    CHECK(f.router.getMouseX() == 100 && f.router.getMouseY() == 200);  // Input.x/y are set on press, not on up
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_NONE);

    // exactly 500 ms is NOT a click (< 500)
    r.log.clear();
    f.router.touchDown(10, 10, 0, LEFT);
    advance(500);
    f.router.touchUp(10, 10, 0, LEFT);
    CHECK_LOG(r, "press(0,10,10,0)", "release(0,10,10,0)");

    // 499 ms is a click
    r.log.clear();
    f.router.touchDown(10, 10, 0, LEFT);
    advance(499);
    f.router.touchUp(10, 10, 0, LEFT);
    CHECK_LOG(r, "press(0,10,10,0)", "click(0,10,10,1)", "release(0,10,10,0)");

    // moved 10 px: dx*dx = 100, not < 100 -> no click; 9 px -> click
    r.log.clear();
    f.router.touchDown(50, 50, 0, LEFT);
    advance(50);
    f.router.touchUp(60, 50, 0, LEFT);
    CHECK_LOG(r, "press(0,50,50,0)", "release(0,60,50,0)");
    r.log.clear();
    f.router.touchDown(50, 50, 0, LEFT);
    advance(50);
    f.router.touchUp(59, 50, 0, LEFT);
    CHECK_LOG(r, "press(0,50,50,0)", "click(0,59,50,1)", "release(0,59,50,0)");

    // a drag away and back to the start still counts as a click (the Java only compares the up position with the down position)
    r.log.clear();
    f.router.touchDown(70, 70, 0, LEFT);
    f.router.touchDragged(300, 70, 0);
    f.router.touchDragged(70, 70, 0);
    advance(100);
    f.router.touchUp(70, 70, 0, LEFT);
    CHECK_LOG(r, "press(0,70,70,0)", "drag(70,70,300,70)", "drag(300,70,70,70)", "click(0,70,70,1)", "release(0,70,70,0)");

    // the button passed to touchDown/touchUp is forwarded for pointer 0
    r.log.clear();
    f.router.touchDown(5, 6, 0, input::Buttons::MIDDLE);
    f.router.touchUp(5, 6, 0, input::Buttons::MIDDLE);
    CHECK_LOG(r, "press(2,5,6,0)", "click(2,5,6,1)", "release(2,5,6,0)");
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. second finger while the first is down -> right button press/release at the first finger's last position (oldx, oldy)
// ---------------------------------------------------------------------------------------------------------------------
static void testRightClick() {
    Rec r; Fixture f; f.current = &r;
    f.router.touchDown(100, 100, 0, LEFT);
    f.router.touchDragged(120, 130, 0);          // oldx,oldy = 120,130
    f.router.touchDown(400, 400, 1, LEFT);       // right press at (120,130), NOT at the finger's own position
    CHECK(f.router.isRightButtonDown());
    CHECK_LOG(r, "press(0,100,100,0)", "drag(100,100,120,130)", "press(1,120,130,0)");
    CHECK(f.router.getMouseX() == 120 && f.router.getMouseY() == 130);  // mousePressed sets Input.x/y to the pressed position
    advance(100);
    r.log.clear();
    f.router.touchUp(401, 401, 1, LEFT);         // up of finger 1: dx,dy = oldx-touchX = 0 -> click, then release, both at oldx,oldy
    CHECK_LOG(r, "click(1,120,130,1)", "release(1,120,130,0)");
    CHECK(!f.router.isRightButtonDown());
    // finger 0 is still down and still works
    r.log.clear();
    f.router.touchDragged(125, 130, 0);
    CHECK_LOG(r, "drag(120,130,125,130)");
    advance(100);
    f.router.touchUp(125, 130, 0, LEFT);
    // the right press overwrote touchX,touchY with oldx,oldy = (120,130) and touchTime with "now"; finger 0 went up 5 px away
    // from there, 100 ms later -> still a click
    CHECK_LOG(r, "drag(120,130,125,130)", "click(0,125,130,1)", "release(0,125,130,0)");

    // a long right "press" is not a click
    r.log.clear();
    f.router.touchDown(0, 0, 0, LEFT);
    f.router.touchDown(0, 0, 2, LEFT);
    advance(600);
    f.router.touchUp(0, 0, 2, LEFT);
    CHECK_LOG(r, "press(0,0,0,0)", "press(1,0,0,0)", "release(1,0,0,0)");

    // a third finger while the right button is already down: release first, then press again (Java: if (rightIsPressed) mouseReleased)
    r.log.clear();
    f.router.touchDown(5, 5, 1, LEFT);
    f.router.touchDown(6, 6, 2, LEFT);
    CHECK_LOG(r, "press(1,0,0,0)", "release(1,0,0,0)", "press(1,0,0,0)");
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. drag with finger 0 only; a finger > 0 moving does nothing
// ---------------------------------------------------------------------------------------------------------------------
static void testDrag() {
    Rec r; Fixture f; f.current = &r;
    f.router.touchDown(10, 10, 0, LEFT);
    f.router.touchDown(50, 50, 1, LEFT);
    r.log.clear();
    f.router.touchDragged(90, 90, 1);  // ignored
    f.router.touchDragged(95, 95, 3);  // ignored (pointer never went down, still ignored)
    CHECK_LOG(r);
    f.router.touchDragged(12, 14, 0);
    f.router.touchDragged(20, 30, 0);
    CHECK_LOG(r, "drag(10,10,12,14)", "drag(12,14,20,30)");
    CHECK(f.router.getMouseX() == 20 && f.router.getMouseY() == 30);
}

// ---------------------------------------------------------------------------------------------------------------------
// 4. a RAW pointer keeps going to its original listener after the state changes; its up/cancel never reaches the menu path
// ---------------------------------------------------------------------------------------------------------------------
static void testRawOwnership() {
    RawRec game; Rec menu; Fixture f; f.current = &game;
    f.router.touchDown(10, 20, 0, LEFT);
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_RAW);
    CHECK(f.router.getRawTarget() == &game);
    f.current = &menu;  // state change while the finger is down
    f.router.touchDragged(11, 21, 0);
    f.router.touchUp(12, 22, 0, LEFT);
    CHECK_RAW(game, "rawDown(0,10,20,0)", "rawMove(0,11,21,0)", "rawUp(0,12,22,0)");
    CHECK_LOG(game);  // no menu events on the gameplay state
    CHECK_LOG(menu);  // and none on the new state: the raw finger is never emulated as a mouse
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_NONE);

    // a raw state that is NOT active (e.g. paused) -> menu path, no raw events
    RawRec paused; paused.active = false; f.current = &paused;
    f.router.touchDown(1, 2, 0, LEFT);
    f.router.touchUp(1, 2, 0, LEFT);
    CHECK_RAW(paused);
    CHECK(paused.log.size() == 3);  // press, click, release
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_NONE);

    // the state turns raw-inactive while a raw finger is down: the finger stays raw
    RawRec g2; f.current = &g2;
    f.router.touchDown(30, 30, 0, LEFT);
    g2.active = false;
    f.router.touchDragged(31, 31, 0);
    f.router.touchUp(31, 31, 0, LEFT);
    CHECK_RAW(g2, "rawDown(0,30,30,0)", "rawMove(0,31,31,0)", "rawUp(0,31,31,0)");
    CHECK_LOG(g2);

    // a finger that goes down in the menu stays a menu finger when the state becomes raw-active
    RawRec g3; g3.active = false; f.current = &g3;
    f.router.touchDown(40, 40, 0, LEFT);
    g3.active = true;
    f.router.touchDragged(41, 41, 0);
    f.router.touchUp(41, 41, 0, LEFT);
    CHECK_RAW(g3);
    CHECK_LOG(g3, "press(0,40,40,0)", "drag(40,40,41,41)", "click(0,41,41,1)", "release(0,41,41,0)");

    // pointer >= 20 is not tracked: raw is skipped and it goes the menu way (Java: validPointer)
    RawRec g4; f.current = &g4;
    f.router.touchDown(1, 1, 25, LEFT);
    CHECK_RAW(g4);
    CHECK_LOG(g4, "press(1,41,41,0)");  // pointer > 0 -> right click at oldx,oldy (= where finger 0 last was: 41,41 from g3 above)
    f.router.touchUp(1, 1, 25, LEFT);
}

// Documented quirk (kept from the Java): there is ONE rawTarget field, written on every raw DOWN. If the state changes
// while finger 1 is down and finger 2 goes down on the new raw state, finger 1's up goes to the NEW listener.
static void testSingleRawTargetQuirk() {
    RawRec a, b; Fixture f; f.current = &a;
    f.router.touchDown(1, 1, 0, LEFT);
    f.current = &b;
    f.router.touchDown(2, 2, 1, LEFT);
    f.router.touchUp(1, 1, 0, LEFT);
    CHECK_RAW(a, "rawDown(0,1,1,0)");
    CHECK_RAW(b, "rawDown(1,2,2,0)", "rawUp(0,1,1,0)");
}

// ---------------------------------------------------------------------------------------------------------------------
// 5. cancel affects only the cancelled RAW finger; with the real GameplayInput behind it, two fingers = two cursors
// ---------------------------------------------------------------------------------------------------------------------
struct GameplayAdapter : RawTouchListener {
    GameplayInput gi;
    bool isRawTouchActive() override { return true; }
    void rawTouchDown(int p, int x, int y) override { gi.press(p, x, y); }
    void rawTouchMoved(int p, int x, int y) override { gi.move(p, x, y); }
    void rawTouchUp(int p, int x, int y) override { gi.release(p, x, y); }
    void rawTouchCancelled(int p) override { gi.release(p, gi.getCursor(p) ? gi.getCursor(p)->getX() : 0, gi.getCursor(p) ? gi.getCursor(p)->getY() : 0); }
};
struct GameState : Rec, GameplayAdapter {};  // diamond-free: Rec is the InputTarget, GameplayAdapter the RawTouchListener

static void testCancel() {
    RawRec g; Rec menu; Fixture f; f.current = &g;
    f.router.touchDown(10, 10, 0, LEFT);
    f.router.touchDown(20, 20, 1, LEFT);
    f.router.touchCancelled(0, 0, 1, LEFT);  // the position of a cancel is meaningless
    CHECK_RAW(g, "rawDown(0,10,10,0)", "rawDown(1,20,20,0)", "rawCancel(1,0,0,0)");
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_RAW);
    CHECK(f.router.getOwner(1) == TouchRouter::OWNER_NONE);
    f.router.touchDragged(11, 11, 0);  // finger 0 is untouched by the cancel
    f.router.touchUp(11, 11, 0, LEFT);
    CHECK_RAW(g, "rawDown(0,10,10,0)", "rawDown(1,20,20,0)", "rawCancel(1,0,0,0)", "rawMove(0,11,11,0)", "rawUp(0,11,11,0)");
    CHECK_LOG(g);

    // a cancel of a finger that is already gone does nothing on the raw side and falls to the menu path (like the Java)
    g.raw.clear();
    f.current = &menu;
    f.router.touchCancelled(0, 0, 1, LEFT);
    CHECK_RAW(g);
    // touchUp on pointer>0 at oldx,oldy = (0,0). No click: nothing in this fixture ever pressed on the menu path, so touchTime is
    // still 0 and "now - touchTime < 500" is false.
    CHECK_LOG(menu, "release(1,0,0,0)");
}

static void testCancelMenu() {
    // a cancelled MENU finger behaves like touchUp (so a stuck mouse/right button gets released)
    Rec r; Fixture f; f.current = &r;
    f.router.touchDown(100, 100, 0, LEFT);
    f.router.touchDown(5, 5, 1, LEFT);
    advance(1000);  // long: no click
    r.log.clear();
    f.router.touchCancelled(0, 0, 1, LEFT);
    CHECK_LOG(r, "release(1,100,100,0)");
    CHECK(!f.router.isRightButtonDown());
    f.router.touchCancelled(100, 100, 0, LEFT);
    CHECK_LOG(r, "release(1,100,100,0)", "release(0,100,100,0)");
}

static void testTwoCursorsWithGameplayInput() {
    GameState g; Fixture f; f.current = &g;
    f.router.touchDown(10, 10, 0, LEFT);
    f.router.touchDown(300, 200, 1, LEFT);
    CHECK(g.gi.getDownCount() == 2);
    CHECK(g.gi.getCursor(0)->isDown() && g.gi.getCursor(1)->isDown());
    CHECK(g.gi.getCursor(0)->getX() == 10 && g.gi.getCursor(1)->getX() == 300);  // independent positions
    f.router.touchDragged(15, 12, 0);
    CHECK(g.gi.getCursor(0)->getX() == 15 && g.gi.getCursor(1)->getX() == 300 && g.gi.getCursor(1)->getY() == 200);
    f.router.touchCancelled(0, 0, 0, LEFT);  // cancel finger 0 only
    CHECK(!g.gi.getCursor(0)->isDown());
    CHECK(g.gi.getCursor(1)->isDown());
    CHECK(g.gi.getDownCount() == 1);
    CHECK(g.log.empty());  // nothing was emulated as a mouse
    f.router.touchUp(300, 200, 1, LEFT);
    CHECK(g.gi.getDownCount() == 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// 6. inputLocked swallows touchDown / scroll / key down / key typed; a throwing target does not crash the router
// Java: only touchDown, scrolled, keyDown, keyTyped test inputLocked(); touchUp, touchDragged, touchCancelled and keyUp do NOT
// (so a finger that went down before the lock is still released). The same here.
// ---------------------------------------------------------------------------------------------------------------------
static void testLocked() {
    Rec r; Fixture f; f.current = &r;
    f.router.touchDown(1, 1, 0, LEFT);  // down before the lock
    f.locked = true;
    CHECK(f.router.touchDown(2, 2, 1, LEFT) == true);   // swallowed: returns true like the Java
    CHECK(f.router.scrolled(0, 1) == true);
    CHECK(f.router.keyDown(29) == true);
    CHECK(f.router.keyTyped('a') == true);
    CHECK_LOG(r, "press(0,1,1,0)");                      // nothing new reached the state
    CHECK(f.router.getOwner(1) == TouchRouter::OWNER_NONE);
    CHECK(!f.router.isRightButtonDown());
    // releases are not blocked
    advance(10);
    f.router.touchUp(1, 1, 0, LEFT);
    f.router.keyUp(29);
    CHECK_LOG(r, "press(0,1,1,0)", "click(0,1,1,1)", "release(0,1,1,0)", "keyUp(29,0,0,0)");
    f.locked = false;
    CHECK(f.router.keyDown(29) == false);
    CHECK_LOG(r, "press(0,1,1,0)", "click(0,1,1,1)", "release(0,1,1,0)", "keyUp(29,0,0,0)", "keyDown(29,0,0,0)");

    // a raw finger down before the lock is released normally; one that tries to go down during the lock never becomes raw
    RawRec g; f.current = &g;
    f.router.touchDown(5, 5, 0, LEFT);
    f.locked = true;
    f.router.touchDown(6, 6, 1, LEFT);
    f.router.touchUp(5, 5, 0, LEFT);
    CHECK_RAW(g, "rawDown(0,5,5,0)", "rawUp(0,5,5,0)");
    CHECK(f.router.getOwner(1) == TouchRouter::OWNER_NONE);
}

static void testThrowingTarget() {
    Rec r; Fixture f; f.current = &r;
    r.throwOnPress = true;
    CHECK(f.router.touchDown(1, 2, 0, LEFT) == false);  // must not throw
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_MENU);
    // like the Java (exception inside the try): touch state was not updated after the failed press
    r.throwOnPress = false;
    r.log.clear();

    // a throwing mouseClicked must not leave the right button stuck down
    r.throwOnClick = true;
    f.router.touchDown(10, 10, 0, LEFT);
    f.router.touchDown(10, 10, 1, LEFT);
    CHECK(f.router.isRightButtonDown());
    f.router.touchUp(10, 10, 1, LEFT);
    CHECK(!f.router.isRightButtonDown());
    f.router.touchUp(10, 10, 0, LEFT);
    r.throwOnClick = false;

    // a throwing raw listener does not break the owner table
    struct ThrowRaw : RawRec {
        void rawTouchDown(int, int, int) override { throw std::runtime_error("down"); }
        void rawTouchMoved(int, int, int) override { throw std::runtime_error("move"); }
        void rawTouchUp(int, int, int) override { throw std::runtime_error("up"); }
        void rawTouchCancelled(int) override { throw std::runtime_error("cancel"); }
    } t;
    f.current = &t;
    f.router.touchDown(1, 1, 0, LEFT);
    f.router.touchDragged(2, 2, 0);
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_RAW);
    f.router.touchCancelled(0, 0, 0, LEFT);
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_NONE);
    f.router.touchDown(1, 1, 0, LEFT);
    f.router.touchUp(1, 1, 0, LEFT);
    CHECK(f.router.getOwner(0) == TouchRouter::OWNER_NONE);

    // keys / scroll
    struct ThrowKey : Rec {
        void keyPressed(int, char) override { throw std::runtime_error("k"); }
        void mouseWheelMoved(int) override { throw 42; }  // not even a std::exception
    } k;
    f.current = &k;
    CHECK(f.router.keyDown(1) == false);
    CHECK(f.router.scrolled(0, 1) == false);

    // no target at all
    f.current = nullptr;
    f.router.touchDown(1, 1, 0, LEFT);
    f.router.touchUp(1, 1, 0, LEFT);
    f.router.scrolled(0, 1);
    f.router.keyDown(1);
}

// ---------------------------------------------------------------------------------------------------------------------
// scroll, keys, state change
// ---------------------------------------------------------------------------------------------------------------------
static void testScrollKeysState() {
    Rec r; Fixture f; f.current = &r;
    // Java: currentState.mouseWheelMoved((int) -amountY): amountY > 0 is a scroll DOWN in libGDX, so the state gets a negative value
    f.router.scrolled(5, 1);
    f.router.scrolled(0, -2);
    f.router.scrolled(0, 0.5f);   // (int) -0.5f == 0
    f.router.scrolled(0, -2.9f);  // (int) 2.9f == 2 (truncation)
    CHECK_LOG(r, "wheel(-1,0,0,0)", "wheel(2,0,0,0)", "wheel(0,0,0,0)", "wheel(2,0,0,0)");
    r.log.clear();
    CHECK(f.router.keyDown(4) == false);  // keys: char is always 0 (Java: (char) 0)
    f.router.keyUp(4);
    CHECK_LOG(r, "keyDown(4,0,0,0)", "keyUp(4,0,0,0)");

    // Java enterNextState: touchX = touchY = 0 (touchTime is not reset). A finger down in the old state then lifted in the new one
    // right at (0,0) counts as unmoved.
    r.log.clear();
    f.router.touchDown(500, 500, 0, LEFT);
    f.router.onStateEntered();
    advance(10);
    f.router.touchUp(3, 3, 0, LEFT);  // dx,dy = 3,3 from (0,0) -> click
    CHECK_LOG(r, "press(0,500,500,0)", "click(0,3,3,1)", "release(0,3,3,0)");
}

// ---------------------------------------------------------------------------------------------------------------------
// 7. the Android slot allocator (pure part): ids 7 and 3 get slots 0 and 1, slots are reused after up, never more than 20
// ---------------------------------------------------------------------------------------------------------------------
static void testSlots() {
    PointerSlots s;
    CHECK(s.acquire(7) == 0);
    CHECK(s.acquire(3) == 1);
    CHECK(s.acquire(7) == 0);  // already active: same slot
    CHECK(s.find(3) == 1 && s.find(99) == -1);
    CHECK(s.activeCount() == 2);
    CHECK(s.release(7) == 0);
    CHECK(s.release(7) == -1);  // already free
    CHECK(s.acquire(11) == 0);  // lowest free slot is reused (slot 0 is "the mouse" again)
    CHECK(s.idAt(0) == 11 && s.idAt(1) == 3);
    CHECK(s.release(3) == 1);
    CHECK(s.acquire(5) == 1);
    s.clear();
    CHECK(s.activeCount() == 0);
    for (int i = 0; i < PointerSlots::MAX; i++) CHECK(s.acquire(100 + i) == i);
    CHECK(s.activeCount() == 20);
    CHECK(s.acquire(999) == -1);  // 21st finger: no slot
    CHECK(s.find(999) == -1);
    CHECK(s.release(105) == 5);
    CHECK(s.acquire(999) == 5);   // frees up -> usable again
    CHECK(s.acquire(-1) == -1);   // invalid id
    CHECK(s.idAt(-1) == -1 && s.idAt(20) == -1);
}

int main() {
    testTap();
    testRightClick();
    testDrag();
    testRawOwnership();
    testSingleRawTargetQuirk();
    testCancel();
    testCancelMenu();
    testTwoCursorsWithGameplayInput();
    testLocked();
    testThrowingTarget();
    testScrollKeysState();
    testSlots();
    if (failures == 0) std::printf("test_input_t4: all passed\n");
    else std::printf("test_input_t4: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
