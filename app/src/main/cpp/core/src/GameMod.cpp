#include "opsu/GameMod.h"
#include "opsu/input/Keys.h"

namespace opsu {

namespace K = input::Keys;
using C = GameMod::Category;

GameMod GameMod::EASY(C::EASY, 0, "EZ", 2, K::Q, 0.5f, "Easy",
                      "Reduces overall difficulty - larger circles, more forgiving HP drain, less accuracy required.");
GameMod GameMod::NO_FAIL(C::EASY, 1, "NF", 1, K::W, 0.5f, "NoFail", "You can't fail.  No matter what.");
GameMod GameMod::HALF_TIME(C::EASY, 2, "HT", 256, K::E, 0.3f, "HalfTime", "Less zoom.");
GameMod GameMod::HARD_ROCK(C::HARD, 0, "HR", 16, K::A, 1.06f, "HardRock", "Everything just got a bit harder...");
GameMod GameMod::SUDDEN_DEATH(C::HARD, 1, "SD", 32, K::S, 1.f, "SuddenDeath", "Miss a note and fail.");
GameMod GameMod::DOUBLE_TIME(C::HARD, 2, "DT", 64, K::D, 1.12f, "DoubleTime", "Zoooooooooom.");
GameMod GameMod::NIGHTCORE(C::HARD, 3, "NC", 512, K::N, 1.12f, "Nightcore", "uguuuuuuuu");
GameMod GameMod::HIDDEN(C::HARD, 4, "HD", 8, K::F, 1.06f, "Hidden",
                        "Play with no approach circles and fading notes for a slight score advantage.");
GameMod GameMod::FLASHLIGHT(C::HARD, 5, "FL", 1024, K::G, 1.12f, "Flashlight", "Restricted view area.");
GameMod GameMod::RELAX(C::SPECIAL, 0, "RL", 128, K::Z, 0.f, "Relax",
                       "You don't need to click.\nGive your clicking/tapping finger a break from the heat of things.\n**UNRANKED**");
GameMod GameMod::AUTOPILOT(C::SPECIAL, 1, "AP", 8192, K::X, 0.f, "Relax2",
                           "Automatic cursor movement - just follow the rhythm.\n**UNRANKED**");
GameMod GameMod::SPUN_OUT(C::SPECIAL, 2, "SO", 4096, K::C, 0.9f, "SpunOut",
                          "Spinners will be automatically completed.");
GameMod GameMod::AUTO(C::SPECIAL, 3, "", 2048, K::V, 1.f, "Autoplay",
                      "Watch a perfect automated play through the song.");

namespace {
float g_scoreMultiplier = -1.f, g_speedMultiplier = -1.f, g_difficultyMultiplier = -1.f;
void invalidate() { g_scoreMultiplier = g_speedMultiplier = g_difficultyMultiplier = -1.f; }
}  // namespace

const char* GameMod::categoryName(Category c) {
    switch (c) {
        case Category::EASY: return "Difficulty Reduction";
        case Category::HARD: return "Difficulty Increase";
        default: return "Special";
    }
}

const std::array<GameMod*, GameMod::SIZE>& GameMod::values() {
    static const std::array<GameMod*, SIZE> v = {&EASY, &NO_FAIL, &HALF_TIME, &HARD_ROCK, &SUDDEN_DEATH,
                                                 &DOUBLE_TIME, &NIGHTCORE, &HIDDEN, &FLASHLIGHT, &RELAX,
                                                 &AUTOPILOT, &SPUN_OUT, &AUTO};
    return v;
}

const std::array<GameMod*, GameMod::SIZE>& GameMod::valuesReversed() {
    static const std::array<GameMod*, SIZE> v = [] {
        std::array<GameMod*, SIZE> r = values();
        for (int i = 0; i < SIZE / 2; i++) std::swap(r[i], r[SIZE - 1 - i]);
        return r;
    }();
    return v;
}

void GameMod::resetState() {
    for (GameMod* m : values()) m->active_ = false;
    invalidate();
}

float GameMod::getScoreMultiplier() {
    if (g_scoreMultiplier < 0.f) {
        float multiplier = 1.f;
        for (GameMod* mod : values())
            if (mod->isActive()) multiplier *= mod->getMultiplier();
        g_scoreMultiplier = multiplier;
    }
    return g_scoreMultiplier;
}

float GameMod::getSpeedMultiplier() {
    if (g_speedMultiplier < 0.f) {
        if (DOUBLE_TIME.isActive() || NIGHTCORE.isActive()) g_speedMultiplier = 1.5f;
        else if (HALF_TIME.isActive()) g_speedMultiplier = 0.75f;
        else g_speedMultiplier = 1.f;
    }
    return g_speedMultiplier;
}

float GameMod::getDifficultyMultiplier() {
    if (g_difficultyMultiplier < 0.f) {
        if (HARD_ROCK.isActive()) g_difficultyMultiplier = 1.4f;
        else if (EASY.isActive()) g_difficultyMultiplier = 0.5f;
        else g_difficultyMultiplier = 1.f;
    }
    return g_difficultyMultiplier;
}

int GameMod::getModState() {
    int state = 0;
    for (GameMod* mod : values())
        if (mod->isActive()) state |= mod->getBit();
    return state;
}

void GameMod::loadModState(int state) {
    invalidate();
    for (GameMod* mod : values()) mod->active_ = ((state & mod->getBit()) > 0);
    // osu! stores Nightcore as DoubleTime | Nightcore: keep only Nightcore
    if (NIGHTCORE.active_) DOUBLE_TIME.active_ = false;
}

std::string GameMod::getModString(int state) {
    std::string sb;
    for (GameMod* mod : values())
        if ((state & mod->getBit()) > 0) { sb += mod->getName(); sb += ','; }
    if (!sb.empty()) { sb.pop_back(); return sb; }
    return "None";
}

void GameMod::toggle(bool checkInverse) {
    active_ = !active_;
    invalidate();

    if (checkInverse) {
        if (AUTO.isActive()) {
            if (this == &AUTO) {
                SPUN_OUT.active_ = false;
                SUDDEN_DEATH.active_ = false;
                RELAX.active_ = false;
                AUTOPILOT.active_ = false;
            } else if (this == &SPUN_OUT || this == &SUDDEN_DEATH || this == &RELAX || this == &AUTOPILOT)
                active_ = false;
        }
        if (active_ && (this == &SUDDEN_DEATH || this == &NO_FAIL || this == &RELAX || this == &AUTOPILOT)) {
            SUDDEN_DEATH.active_ = false;
            NO_FAIL.active_ = false;
            RELAX.active_ = false;
            AUTOPILOT.active_ = false;
            active_ = true;
        }
        if (AUTOPILOT.isActive() && SPUN_OUT.isActive()) {
            if (this == &AUTOPILOT) SPUN_OUT.active_ = false;
            else AUTOPILOT.active_ = false;
        }
        if (EASY.isActive() && HARD_ROCK.isActive()) {
            if (this == &EASY) HARD_ROCK.active_ = false;
            else EASY.active_ = false;
        }
        if (active_ && (this == &HALF_TIME || this == &DOUBLE_TIME || this == &NIGHTCORE)) {
            // HalfTime, DoubleTime and Nightcore are mutually exclusive
            if (this != &HALF_TIME) HALF_TIME.active_ = false;
            if (this != &DOUBLE_TIME) DOUBLE_TIME.active_ = false;
            if (this != &NIGHTCORE) NIGHTCORE.active_ = false;
        }
    }
}

}  // namespace opsu
