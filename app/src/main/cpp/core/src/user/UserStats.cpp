#include "opsu/user/UserStats.h"
#include <algorithm>
#include "opsu/GameMod.h"

namespace opsu {

void UserStats::Play::prepare() {
    accuracy = GameData::getScorePercent(hit300, hit100, hit50, miss);
    bool silver = (mods & GameMod::HIDDEN.getBit()) > 0 || (mods & GameMod::FLASHLIGHT.getBit()) > 0;
    grade = GameData::getGrade(hit300, hit100, hit50, miss, silver);
}

void UserStats::add(Play p) {
    p.prepare();
    plays++;
    hit300 += p.hit300;
    hit100 += p.hit100;
    hit50 += p.hit50;
    miss += p.miss;
    if (p.score > bestScore) bestScore = p.score;
    if (p.combo > bestCombo) bestCombo = p.combo;
    if (p.perfect) fullCombos++;
    accuracySum += p.accuracy;
    if (firstPlayed == 0 || p.timestamp < firstPlayed) firstPlayed = p.timestamp;
    if (p.timestamp > lastPlayed) lastPlayed = p.timestamp;
    using G = GameData::Grade;
    switch (p.grade) {
        case G::SS: case G::SSH: gradeSS++; break;
        case G::S: case G::SH: gradeS++; break;
        case G::A: gradeA++; break;
        case G::B: gradeB++; break;
        case G::C: gradeC++; break;
        case G::D: gradeD++; break;
        default: break;
    }

    // keep only the best plays
    topPlays.push_back(std::move(p));
    if (static_cast<int>(topPlays.size()) > MAX_TOP_PLAYS * 2) trimTopPlays();
}

void UserStats::finish() {
    averageAccuracy = (plays > 0) ? static_cast<float>(accuracySum / plays) : 0.f;
    trimTopPlays();
}

void UserStats::trimTopPlays() {
    // Collections.sort is stable; so is this. Highest score first.
    std::stable_sort(topPlays.begin(), topPlays.end(), [](const Play& a, const Play& b) { return a.score > b.score; });
    while (static_cast<int>(topPlays.size()) > MAX_TOP_PLAYS) topPlays.pop_back();
}

}  // namespace opsu
