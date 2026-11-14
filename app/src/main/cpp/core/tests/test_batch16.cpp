// Tests for batch 16 (TouchCursor + GameplayInput). Behavioural, no JVM golden (integer state machine; one float distance test).
#include <cstdio>
#include <string>
#include "opsu/input/GameplayInput.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// Records every call so we can check what GameplayInput does to the per-finger sprites.
struct Rec : CursorSprite {
    std::string* log; int id;
    Rec(std::string* l, int i) : log(l), id(i) {}
    void resetLocations() override { *log += "r" + std::to_string(id) + ";"; }
    void update(int d) override { *log += "u" + std::to_string(id) + ":" + std::to_string(d) + ";"; }
    void draw(int x, int y, bool p) override {
        *log += "d" + std::to_string(id) + ":" + std::to_string(x) + "," + std::to_string(y) + (p ? "t" : "f") + ";";
    }
};

int main() {
    // pool + defaults
    {
        GameplayInput in;
        CHECK(GameplayInput::MAX_POINTERS == 20 && in.getCursors().size() == 20);
        CHECK(in.getMaxActive() == 3 && !in.isAnyDown() && in.getDownCount() == 0);
        CHECK(in.getMainId() == -1 && in.getMain() == nullptr && !in.hasPosition());
        CHECK(in.getMainX() == 0 && in.getMainY() == 0);
        for (int i = 0; i < 20; i++) CHECK(in.getCursor(i)->id == i && in.getCursor(i)->keyBit == -1 && !in.getCursor(i)->isDown());
        CHECK(in.getCursor(-1) == nullptr && in.getCursor(20) == nullptr);
        CHECK(in.getOldestDown() == nullptr);
    }
    // setMaxActive clamps to 1..20
    {
        GameplayInput in;
        in.setMaxActive(0);   CHECK(in.getMaxActive() == 1);
        in.setMaxActive(-5);  CHECK(in.getMaxActive() == 1);
        in.setMaxActive(7);   CHECK(in.getMaxActive() == 7);
        in.setMaxActive(20);  CHECK(in.getMaxActive() == 20);
        in.setMaxActive(99);  CHECK(in.getMaxActive() == 20);
    }
    // press / move / release basics
    {
        GameplayInput in;
        TouchCursor* a = in.press(4, 10, 20);
        CHECK(a != nullptr && a == in.getCursor(4) && a->isDown() && a->getX() == 10 && a->getY() == 20);
        CHECK(a->getDownX() == 10 && a->getDownY() == 20);
        CHECK(in.getMainId() == 4 && in.getMain() == a && in.isAnyDown() && in.getDownCount() == 1);
        CHECK(in.hasPosition() && in.getMainX() == 10 && in.getMainY() == 20);
        CHECK(in.press(4, 99, 99) == nullptr);                        // duplicate DOWN without UP
        CHECK(a->getX() == 10 && a->getDownX() == 10 && in.getDownCount() == 1);
        CHECK(in.move(4, 30, 40) && a->getX() == 30 && a->getY() == 40 && a->getDownX() == 10);
        CHECK(in.getMainX() == 30 && in.getMainY() == 40);
        CHECK(!in.move(5, 1, 1) && !in.move(-1, 1, 1) && !in.move(20, 1, 1));
        TouchCursor* r = in.release(4, 50, 60);
        CHECK(r == a && !a->isDown() && a->getX() == 50 && a->getY() == 60);
        CHECK(in.getDownCount() == 0 && in.getMainId() == -1 && !in.isAnyDown());
        CHECK(in.getMainX() == 50 && in.getMainY() == 60 && in.hasPosition());   // last position kept
        CHECK(in.release(4, 0, 0) == nullptr && in.release(-1, 0, 0) == nullptr && in.release(20, 0, 0) == nullptr);
        CHECK(in.press(-1, 0, 0) == nullptr && in.press(20, 0, 0) == nullptr);
        CHECK(!in.move(4, 1, 1));                                     // up: move ignored
    }
    // finger limit: extra fingers are ignored until lifted; ignored never becomes gameplay
    {
        GameplayInput in;                                              // limit 3
        CHECK(in.press(0, 1, 1) && in.press(1, 2, 2) && in.press(2, 3, 3));
        CHECK(in.press(3, 4, 4) == nullptr && in.getCursor(3)->isIgnored() && !in.getCursor(3)->isDown());
        CHECK(in.getDownCount() == 3 && in.getMainId() == 2);          // rejected finger does not take over main
        CHECK(!in.move(3, 9, 9));
        CHECK(in.release(3, 9, 9) == nullptr && !in.getCursor(3)->isIgnored());   // release clears 'ignored'
        CHECK(in.release(1, 0, 0) != nullptr);
        CHECK(in.press(3, 4, 4) != nullptr && in.getMainId() == 3);    // room again
        in.setMaxActive(1);                                            // lowering the limit does not kick fingers out
        CHECK(in.getDownCount() == 3 && in.press(7, 0, 0) == nullptr);
    }
    // ignore(): only a finger that is not down
    {
        GameplayInput in;
        in.ignore(2);  CHECK(in.getCursor(2)->isIgnored());
        in.ignore(-1); in.ignore(20);                                  // no crash
        in.press(5, 0, 0);
        in.ignore(5);  CHECK(!in.getCursor(5)->isIgnored() && in.getCursor(5)->isDown());
        CHECK(in.press(2, 7, 7) != nullptr && !in.getCursor(2)->isIgnored());   // a later press clears the ignore flag
        in.ignore(9);
        in.release(9, 0, 0);  CHECK(!in.getCursor(9)->isIgnored());
    }
    // main cursor = most recently pressed finger still down; falls back on release
    {
        GameplayInput in;
        in.press(7, 1, 1); in.press(3, 2, 2); in.press(9, 3, 3);       // press order 7, 3, 9
        CHECK(in.getMainId() == 9 && in.getOldestDown()->id == 7);
        in.release(3, 0, 0);                                           // non-main release: main unchanged
        CHECK(in.getMainId() == 9);
        in.release(9, 5, 6);                                           // main release -> most recent still down = 7
        CHECK(in.getMainId() == 7 && in.getMain()->id == 7);
        CHECK(in.getMainX() == 1 && in.getMainY() == 1);               // main is now finger 7, not the last seen position
        CHECK(in.getOldestDown()->id == 7);
        in.release(7, 8, 9);
        CHECK(in.getMainId() == -1 && in.getMainX() == 8 && in.getMainY() == 9 && in.getOldestDown() == nullptr);
        // re-pressing an older id makes it the newest
        in.press(3, 0, 0); in.press(7, 0, 0);
        in.release(3, 0, 0); in.press(3, 0, 0);
        CHECK(in.getMainId() == 3 && in.getOldestDown()->id == 7);
    }
    // move of a non-main finger does not change the remembered position; main move does
    {
        GameplayInput in;
        in.press(0, 1, 1); in.press(1, 2, 2);                          // main = 1
        in.move(0, 50, 50);
        CHECK(in.getMainX() == 2 && in.getMainY() == 2);
        in.move(1, 60, 61);
        CHECK(in.getMainX() == 60 && in.getMainY() == 61);
        in.release(1, 70, 71);                                         // main -> 0, which is at (50,50)
        CHECK(in.getMainId() == 0 && in.getMainX() == 50 && in.getMainY() == 50);
    }
    // position memory after reset(): updated by moves of the main finger only (not by other fingers' moves)
    {
        GameplayInput in;
        in.press(0, 1, 1);
        in.move(0, 50, 51);
        in.reset();
        CHECK(in.getMainId() == -1 && in.getMainX() == 50 && in.getMainY() == 51);
        in.press(0, 1, 1); in.press(1, 2, 2);                          // main = 1
        in.move(0, 70, 71);                                            // non-main move: memory untouched
        in.reset();
        CHECK(in.getMainX() == 2 && in.getMainY() == 2);
    }
    // keyBit: reset on press, kept on release, reset by reset()
    {
        GameplayInput in;
        TouchCursor* c = in.press(1, 0, 0);
        c->keyBit = 1;
        in.release(1, 0, 0);  CHECK(in.getCursor(1)->keyBit == 1);
        c = in.press(1, 0, 0); CHECK(c->keyBit == -1);
        c->keyBit = 0;
        in.reset();
        CHECK(in.getCursor(1)->keyBit == -1 && !in.getCursor(1)->isDown() && in.getDownCount() == 0 && in.getMainId() == -1);
    }
    // reset(): forgets fingers and ignored flags, keeps position memory and the press counter
    {
        GameplayInput in;
        in.press(0, 4, 5); in.press(1, 6, 7); in.press(2, 8, 9); in.press(3, 1, 1);   // 3 rejected -> ignored
        in.reset();
        CHECK(!in.isAnyDown() && in.getDownCount() == 0 && in.getMainId() == -1);
        CHECK(!in.getCursor(3)->isIgnored());
        CHECK(in.hasPosition() && in.getMainX() == 8 && in.getMainY() == 9);          // memory kept (position of last press)
        CHECK(in.press(0, 1, 1) != nullptr && in.getDownCount() == 1);                // limit counter was reset too
        CHECK(!in.move(1, 0, 0));
    }
    // reset on a finger does not clear x/y/pressOrder (Java quirk kept): visible through getX
    {
        GameplayInput in;
        in.press(2, 11, 12);
        in.reset();
        CHECK(in.getCursor(2)->getX() == 11 && in.getCursor(2)->getDownY() == 12);
    }
    // findDownWithin: nearest, float math, strictly inside the radius, ignores fingers that are up
    {
        GameplayInput in;
        in.press(0, 100, 100); in.press(1, 110, 100); in.press(2, 300, 300);
        CHECK(in.findDownWithin(104, 100, 50) == in.getCursor(0));      // d2 = 16 vs 36
        CHECK(in.findDownWithin(108, 100, 50) == in.getCursor(1));
        CHECK(in.findDownWithin(105, 100, 50) == in.getCursor(0));      // tie: first (lowest id) wins, strict <
        CHECK(in.isAnyDownWithin(100, 100, 1) && !in.isAnyDownWithin(200, 200, 10));
        CHECK(in.findDownWithin(100, 100, 0) == nullptr);               // radius 0: nothing is strictly inside
        CHECK(in.findDownWithin(110, 100, 10)->id == 1);                // exactly at the centre
        CHECK(in.findDownWithin(103, 104, 5) == nullptr);               // d2 = 25 vs 25: on the circle -> not inside (id 0 at (100,100))
        CHECK(in.findDownWithin(103, 104, 5.0001f) == in.getCursor(0));
        in.release(0, 0, 0); in.release(1, 0, 0);
        CHECK(in.findDownWithin(100, 100, 50) == nullptr);
        CHECK(in.findDownWithin(300, 300, 1)->id == 2);
        CHECK(in.findDownWithin(300.5f, 300, 1)->id == 2);
    }
    // sprites: reset on press and on release, update only for fingers that are down, draw at the finger with pressed = true
    {
        std::string log;
        int made = 0;
        GameplayInput in([&]() -> std::unique_ptr<CursorSprite> { return std::make_unique<Rec>(&log, made++); });
        CHECK(made == 20 && log.empty());
        in.press(1, 10, 20);                          CHECK(log == "r1;"); log.clear();
        in.press(1, 10, 20);                          CHECK(log.empty());   // duplicate press touches nothing
        in.press(2, 30, 40); log.clear();
        in.update(16);                                CHECK(log == "u1:16;u2:16;"); log.clear();
        in.move(2, 31, 41);
        in.draw();                                    CHECK(log == "d1:10,20t;d2:31,41t;"); log.clear();
        in.release(1, 12, 22);                        CHECK(log == "r1;"); log.clear();
        in.update(5);                                 CHECK(log == "u2:5;"); log.clear();
        in.draw();                                    CHECK(log == "d2:31,41t;"); log.clear();
        in.release(1, 0, 0);                          CHECK(log.empty());   // not down: nothing
        in.press(0, 0, 0); in.press(3, 0, 0); in.press(4, 0, 0);           // 4th finger rejected (limit 3): no sprite reset
        log.clear(); in.press(5, 0, 0);               CHECK(log.empty());
        in.reset();                                   // every cursor's sprite is reset, in id order
        std::string expect;
        for (int i = 0; i < 20; i++) expect += "r" + std::to_string(i) + ";";
        CHECK(log == expect);
        log.clear(); in.update(1); in.draw();         CHECK(log.empty());   // nothing down after reset
    }
    // a factory returning null (or no factory) gives working no-op sprites
    {
        GameplayInput in([]() -> std::unique_ptr<CursorSprite> { return nullptr; });
        in.press(0, 1, 2); in.update(5); in.draw(); in.release(0, 1, 2); in.reset();
        CHECK(in.getMainX() == 1);
    }

    if (failures == 0) std::printf("batch16 OK\n");
    return failures == 0 ? 0 : 1;
}
