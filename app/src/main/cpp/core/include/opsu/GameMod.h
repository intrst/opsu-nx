#pragma once
#include <array>
#include <string>

namespace opsu {

/**
 * Game mods. Port of itdelatrisu.opsu.GameMod (logic half).
 *
 * Java enum constants become static instances (GameMod::HARD_ROCK.isActive()).
 * NOT ported yet (needs GameImage/MenuButton/Fonts/Color): the mod images and buttons,
 * Category colour/position, init(width,height)'s layout part, draw/contains/hover*.
 * resetState() is the state-resetting part of init().
 */
class GameMod {
public:
    enum class Category { EASY = 0, HARD = 1, SPECIAL = 2 };
    static const char* categoryName(Category c);

    static GameMod EASY, NO_FAIL, HALF_TIME, HARD_ROCK, SUDDEN_DEATH, DOUBLE_TIME, NIGHTCORE, HIDDEN,
        FLASHLIGHT, RELAX, AUTOPILOT, SPUN_OUT, AUTO;

    static constexpr int SIZE = 13;
    /** All mods in declaration order. */
    static const std::array<GameMod*, SIZE>& values();
    /** All mods in reverse order. */
    static const std::array<GameMod*, SIZE>& valuesReversed();

    /** Deactivates all mods and clears cached multipliers (state part of Java's init). */
    static void resetState();
    static float getScoreMultiplier();
    static float getSpeedMultiplier();
    static float getDifficultyMultiplier();
    static int getModState();
    static void loadModState(int state);
    static std::string getModString(int state);

    Category getCategory() const { return category_; }
    int getCategoryIndex() const { return categoryIndex_; }
    const std::string& getAbbreviation() const { return abbrev_; }
    int getBit() const { return bit_; }
    /** Shortcut key (libGDX key code, see input/Keys.h). */
    int getKey() const { return key_; }
    float getMultiplier() const { return multiplier_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    bool isActive() const { return active_; }
    void toggle(bool checkInverse);

private:
    GameMod(Category c, int ci, const char* abbrev, int bit, int key, float mult, const char* name,
            const char* desc)
        : category_(c), categoryIndex_(ci), abbrev_(abbrev), bit_(bit), key_(key), multiplier_(mult),
          name_(name), description_(desc) {}

    Category category_;
    int categoryIndex_;
    std::string abbrev_;
    int bit_;
    int key_;
    float multiplier_;
    std::string name_;
    std::string description_;
    bool active_ = false;
};

}  // namespace opsu
