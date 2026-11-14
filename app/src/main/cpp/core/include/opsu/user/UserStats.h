#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "opsu/GameData.h"

namespace opsu {

/** Gameplay statistics of a user, summed from stored scores. Port of itdelatrisu.opsu.user.UserStats. */
class UserStats {
public:
    /** The number of top plays that are kept. */
    static constexpr int MAX_TOP_PLAYS = 15;

    /** A single stored play. */
    struct Play {
        int64_t timestamp = 0;
        std::string title, artist, version;
        int hit300 = 0, hit100 = 0, hit50 = 0, geki = 0, katu = 0, miss = 0;
        int64_t score = 0;
        int combo = 0, mods = 0;
        bool perfect = false;
        /** Accuracy in percent [0, 100]. */
        float accuracy = 0.f;
        GameData::Grade grade = GameData::Grade::NULL_;
        /** Fills the derived fields. */
        void prepare();
    };

    int plays = 0;
    int64_t hit300 = 0, hit100 = 0, hit50 = 0, miss = 0;
    int64_t bestScore = 0;
    int bestCombo = 0;
    int fullCombos = 0;
    int gradeSS = 0, gradeS = 0, gradeA = 0, gradeB = 0, gradeC = 0, gradeD = 0;
    int64_t firstPlayed = 0, lastPlayed = 0;
    float averageAccuracy = 0.f;
    /** The best plays, highest score first. */
    std::vector<Play> topPlays;

    void add(Play p);
    /** Must be called after the last add(). */
    void finish();
    int64_t getTotalHits() const { return hit300 + hit100 + hit50 + miss; }

private:
    void trimTopPlays();
    double accuracySum = 0;
};

}  // namespace opsu
