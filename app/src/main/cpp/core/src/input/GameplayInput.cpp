#include "opsu/input/GameplayInput.h"
#include <algorithm>

namespace opsu {

GameplayInput::GameplayInput(const SpriteFactory& makeSprite) {
    cursors.reserve(MAX_POINTERS);  // never reallocated: TouchCursor* stay valid
    for (int i = 0; i < MAX_POINTERS; i++)
        cursors.emplace_back(i, makeSprite ? makeSprite() : nullptr);
}

void GameplayInput::setMaxActive(int n) { maxActive = std::max(1, std::min(MAX_POINTERS, n)); }

TouchCursor* GameplayInput::press(int pointer, int x, int y) {
    if (!valid(pointer))
        return nullptr;
    TouchCursor& c = cursors[pointer];
    if (c.down)  // duplicate DOWN without UP: ignore
        return nullptr;
    if (downCount >= maxActive) {
        c.ignored = true;
        return nullptr;
    }
    c.ignored = false;
    c.down = true;
    c.x = c.downX = x;
    c.y = c.downY = y;
    c.pressOrder = ++pressCounter;
    c.keyBit = -1;
    c.sprite->resetLocations();
    downCount++;
    mainId = pointer;
    setLast(x, y);
    return &c;
}

void GameplayInput::ignore(int pointer) {
    if (valid(pointer) && !cursors[pointer].down)
        cursors[pointer].ignored = true;
}

bool GameplayInput::move(int pointer, int x, int y) {
    if (!valid(pointer))
        return false;
    TouchCursor& c = cursors[pointer];
    if (!c.down)
        return false;
    c.x = x;
    c.y = y;
    if (pointer == mainId)
        setLast(x, y);
    return true;
}

TouchCursor* GameplayInput::release(int pointer, int x, int y) {
    if (!valid(pointer))
        return nullptr;
    TouchCursor& c = cursors[pointer];
    c.ignored = false;
    if (!c.down)
        return nullptr;
    c.down = false;
    c.x = x;
    c.y = y;
    downCount--;
    if (pointer == mainId) {
        setLast(x, y);
        mainId = findMostRecentDown();
    }
    c.sprite->resetLocations();
    return &c;
}

void GameplayInput::reset() {
    for (TouchCursor& c : cursors)
        c.reset();
    downCount = 0;
    mainId = -1;
}

int GameplayInput::findMostRecentDown() const {
    int best = -1;
    int64_t bestOrder = -1;
    for (const TouchCursor& c : cursors) {
        if (c.down && c.pressOrder > bestOrder) {
            bestOrder = c.pressOrder;
            best = c.id;
        }
    }
    return best;
}

TouchCursor* GameplayInput::getOldestDown() {
    TouchCursor* best = nullptr;
    for (TouchCursor& c : cursors)
        if (c.down && (best == nullptr || c.pressOrder < best->pressOrder))
            best = &c;
    return best;
}

TouchCursor* GameplayInput::findDownWithin(float x, float y, float radius) {
    float best = radius * radius;
    TouchCursor* found = nullptr;
    for (TouchCursor& c : cursors) {
        if (!c.down)
            continue;
        float dx = c.x - x, dy = c.y - y;  // int - float = float (as in Java)
        float d2 = dx * dx + dy * dy;
        if (d2 < best) {
            best = d2;
            found = &c;
        }
    }
    return found;
}

void GameplayInput::update(int delta) {
    for (TouchCursor& c : cursors)
        if (c.down)
            c.sprite->update(delta);
}

void GameplayInput::draw() {
    for (TouchCursor& c : cursors)
        if (c.down)
            c.sprite->draw(c.x, c.y, true);
}

}  // namespace opsu
