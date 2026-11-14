#include "opsu/GameData.h"
#include <cstdint>

namespace opsu {
namespace GameData {

const char* gradeName(Grade g) {
    switch (g) {
        case Grade::NULL_: return "NULL";
        case Grade::SS: return "SS";
        case Grade::SSH: return "SSH";
        case Grade::S: return "S";
        case Grade::SH: return "SH";
        case Grade::A: return "A";
        case Grade::B: return "B";
        case Grade::C: return "C";
        case Grade::D: return "D";
    }
    return "NULL";
}

// Java int arithmetic wraps; do the same instead of invoking C++ signed-overflow UB.
static int wrapAdd(int a, int b) { return static_cast<int>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
static int wrapMul(int a, int b) { return static_cast<int>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

float getScorePercent(int hit300, int hit100, int hit50, int miss) {
    float percent = 0;
    int objectCount = wrapAdd(wrapAdd(wrapAdd(hit300, hit100), hit50), miss);
    if (objectCount > 0) {
        int num = wrapAdd(wrapAdd(wrapMul(hit300, 300), wrapMul(hit100, 100)), wrapMul(hit50, 50));
        percent = static_cast<float>(num) / (static_cast<float>(objectCount) * 300.f) * 100.f;
    }
    return percent;
}

Grade getGrade(int hit300, int hit100, int hit50, int miss, bool silver) {
    int objectCount = wrapAdd(wrapAdd(wrapAdd(hit300, hit100), hit50), miss);
    if (objectCount < 1) return Grade::NULL_;  // avoid division by zero

    float percent = getScorePercent(hit300, hit100, hit50, miss);
    float hit300ratio = static_cast<float>(hit300) * 100.f / static_cast<float>(objectCount);
    float hit50ratio = static_cast<float>(hit50) * 100.f / static_cast<float>(objectCount);
    bool noMiss = (miss == 0);
    if (percent >= 100.f)
        return (silver) ? Grade::SSH : Grade::SS;
    else if (hit300ratio >= 90.f && hit50ratio < 1.0f && noMiss)
        return (silver) ? Grade::SH : Grade::S;
    else if ((hit300ratio >= 80.f && noMiss) || hit300ratio >= 90.f)
        return Grade::A;
    else if ((hit300ratio >= 70.f && noMiss) || hit300ratio >= 80.f)
        return Grade::B;
    else if (hit300ratio >= 60.f)
        return Grade::C;
    else
        return Grade::D;
}

}  // namespace GameData
}  // namespace opsu
