#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "opsu/input/TouchCursor.h"

namespace opsu {

/**
 * Tracks every finger independently during gameplay, like osu!droid's cursor list.
 * Port of itdelatrisu.opsu.input.GameplayInput. Owns no menu state; the menu mouse emulation and this tracker are separate.
 *
 * Rules (from osu!droid): each pointer id has its own cursor; at most getMaxActive() fingers (default 3) can be down at once
 * and extra fingers are ignored until lifted; a finger rejected on press (HUD button, limit) never becomes a gameplay finger;
 * the "main" cursor is the most recently pressed finger that is still down.
 * Not thread-safe (input and render run on the same thread).
 *
 * Seams / deviations
 * - Java returned `TouchCursor` / `null`; here `TouchCursor*` / nullptr. Pointers stay valid for the object's lifetime (pool of
 *   20, never reallocated), but like Java the object is reused: do not keep it across a release.
 * - `getCursors()` returns the pool as `const std::vector<TouchCursor>&` (Java: the array).
 * - The per-finger `Cursor` sprite is a `CursorSprite` created by a factory given to the constructor (default: no-op sprites).
 * - Not copyable (owns the sprites).
 */
class GameplayInput {
public:
    /** libGDX supports pointer indices 0..19. */
    static constexpr int MAX_POINTERS = 20;

    using SpriteFactory = std::function<std::unique_ptr<CursorSprite>()>;

    explicit GameplayInput(const SpriteFactory& makeSprite = nullptr);
    GameplayInput(const GameplayInput&) = delete;
    GameplayInput& operator=(const GameplayInput&) = delete;

    /** Sets the maximum number of simultaneously active fingers (clamped to 1..MAX_POINTERS). */
    void setMaxActive(int n);
    int getMaxActive() const { return maxActive; }

    /** Registers a finger press. Returns the cursor if accepted, or nullptr if rejected (invalid id, duplicate down, limit reached). */
    TouchCursor* press(int pointer, int x, int y);
    /** Marks a finger as consumed by something else (HUD); its move/up events are ignored. */
    void ignore(int pointer);
    /** Returns true if the move updated a gameplay finger. */
    bool move(int pointer, int x, int y);
    /** Returns the released cursor if it was a gameplay finger, otherwise nullptr. */
    TouchCursor* release(int pointer, int x, int y);
    /** Forgets every finger (state change, cancel, pause). Position memory is kept. */
    void reset();

    bool isAnyDown() const { return downCount > 0; }
    int getDownCount() const { return downCount; }
    TouchCursor* getCursor(int pointer) { return valid(pointer) ? &cursors[pointer] : nullptr; }
    const std::vector<TouchCursor>& getCursors() const { return cursors; }
    /** The finger that has been down the longest, or nullptr if none is down. */
    TouchCursor* getOldestDown();
    /** The main cursor id, or -1 if no finger is down. */
    int getMainId() const { return mainId; }
    /** The main cursor, or nullptr if no finger is down. */
    TouchCursor* getMain() { return mainId >= 0 ? &cursors[mainId] : nullptr; }
    bool hasPosition() const { return hasPos; }
    /** Position of the main cursor, or where it was last seen. */
    int getMainX() const { return mainId >= 0 ? cursors[mainId].x : lastX; }
    int getMainY() const { return mainId >= 0 ? cursors[mainId].y : lastY; }
    /** True if any finger that is down lies within `radius` of (x, y). */
    bool isAnyDownWithin(float x, float y, float radius) { return findDownWithin(x, y, radius) != nullptr; }
    /** The nearest finger that is down within `radius` of (x, y), or nullptr. Float math like Java; strictly inside the radius. */
    TouchCursor* findDownWithin(float x, float y, float radius);

    /** Advances the cursor sprite animations (fingers that are down). */
    void update(int delta);
    /** Draws one cursor per finger that is down. */
    void draw();

private:
    static bool valid(int pointer) { return pointer >= 0 && pointer < MAX_POINTERS; }
    int findMostRecentDown() const;
    void setLast(int x, int y) { lastX = x; lastY = y; hasPos = true; }

    std::vector<TouchCursor> cursors;
    int maxActive = 3;
    int downCount = 0;
    int64_t pressCounter = 0;
    int mainId = -1;
    int lastX = 0, lastY = 0;
    bool hasPos = false;
};

}  // namespace opsu
