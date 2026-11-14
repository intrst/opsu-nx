#pragma once
#include <cstdint>
#include <memory>

namespace opsu {

class GameplayInput;

/**
 * Per-finger cursor sprite. Seam for itdelatrisu.opsu.ui.Cursor (rendering, not ported yet): TouchCursor / GameplayInput only
 * ever call these three methods on it. The backend's Cursor implements this; the default is `NullCursorSprite` (no-ops).
 */
class CursorSprite {
public:
    virtual ~CursorSprite() = default;
    /** Cursor.resetLocations(): forget the trail. */
    virtual void resetLocations() = 0;
    /** Cursor.update(delta): advance the animation by `delta` ms. */
    virtual void update(int delta) = 0;
    /** Cursor.draw(mouseX, mouseY, mousePressed). */
    virtual void draw(int mouseX, int mouseY, bool mousePressed) = 0;
};

/** Sprite that does nothing (used until the rendering backend provides a real one). */
class NullCursorSprite final : public CursorSprite {
public:
    void resetLocations() override {}
    void update(int) override {}
    void draw(int, int, bool) override {}
};

/**
 * State of one finger during gameplay (port of itdelatrisu.opsu.input.TouchCursor).
 * Instances are pooled by GameplayInput (stable addresses, never resized); never keep a pointer across a release.
 *
 * Java package-private fields (down, ignored, x, y, downX, downY, pressOrder, sprite) are private here and only GameplayInput
 * (friend) writes them; the Java public getters are kept. `isIgnored()` is an addition (the Java field was package-visible,
 * which the tests need).
 */
class TouchCursor {
public:
    /** The pointer id (libGDX pointer index). */
    int id;

    /** Replay key bit (0 = M1, 1 = M2) assigned to this finger by the game, or -1. */
    int keyBit = -1;

    TouchCursor(int id, std::unique_ptr<CursorSprite> sprite)
        : id(id), sprite(sprite ? std::move(sprite) : std::make_unique<NullCursorSprite>()) {}

    bool isDown() const { return down; }
    bool isIgnored() const { return ignored; }
    int getX() const { return x; }
    int getY() const { return y; }
    int getDownX() const { return downX; }
    int getDownY() const { return downY; }

private:
    friend class GameplayInput;

    /** Whether this finger is down and accepted for gameplay. */
    bool down = false;
    /** Whether this finger was rejected (HUD touch or finger limit); its events are ignored until lifted. */
    bool ignored = false;
    /** Current position, in screen coordinates. */
    int x = 0, y = 0;
    /** Position where the finger went down. */
    int downX = 0, downY = 0;
    /** Monotonic press counter, used to pick the most recently pressed finger. */
    int64_t pressOrder = 0;
    /** Per-finger cursor sprite, so each finger has its own trail. */
    std::unique_ptr<CursorSprite> sprite;

    void reset() {
        down = false;
        ignored = false;
        keyBit = -1;
        sprite->resetLocations();
    }
};

}  // namespace opsu
