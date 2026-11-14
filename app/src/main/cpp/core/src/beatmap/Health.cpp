#include "opsu/beatmap/Health.h"
#include "opsu/GameData.h"
#include "opsu/Utils.h"

namespace opsu {
namespace {
const float HP_50 = 0.4f, HP_100 = 2.2f, HP_300 = 6.f, HP_100K = 10.f, HP_300K = 10.f, HP_300G = 14.f, HP_MU = 6.f,
            HP_SLIDER10 = 3.f, HP_SLIDER30 = 4.f, HP_SPINNERSPIN = 1.7f, HP_SPINNERBONUS = 2.f;
}

void Health::setModifiers(float drain, float normal, float comboEnd) {
    hpDrainRate = drain;
    hpMultiplierNormal = normal;
    hpMultiplierComboEnd = comboEnd;
}

void Health::update(int delta) {
    float multiplier = static_cast<float>(delta) / 32.f;
    if (healthDisplay < health)
        healthDisplay = Utils::clamp(healthDisplay + (health - healthDisplay) * multiplier, 0.f, health);
    else if (healthDisplay > health) {
        if (health < 10.f) multiplier *= 10.f;
        healthDisplay = Utils::clamp(healthDisplay - (healthDisplay - health) * multiplier, health, HP_MAX);
    }
}

void Health::changeHealth(float value) {
    health = Utils::clamp(health + value, 0.f, HP_MAX);
    healthUncapped += value;
}

void Health::changeHealthForHit(int type) {
    using namespace GameData;
    switch (type) {
        case HIT_MISS: changeHealth(Utils::mapDifficultyRange(hpDrainRate, -6, -25, -40)); break;
        case HIT_50: changeHealth(hpMultiplierNormal * Utils::mapDifficultyRange(hpDrainRate, HP_50 * 8, HP_50, HP_50)); break;
        case HIT_100: changeHealth(hpMultiplierNormal * Utils::mapDifficultyRange(hpDrainRate, HP_100 * 8, HP_100, HP_100)); break;
        case HIT_300: changeHealth(hpMultiplierNormal * HP_300); break;
        case HIT_100K: changeHealth(hpMultiplierComboEnd * HP_100K); break;
        case HIT_300K: changeHealth(hpMultiplierComboEnd * HP_300K); break;
        case HIT_300G: changeHealth(hpMultiplierComboEnd * HP_300G); break;
        case HIT_MU: changeHealth(hpMultiplierNormal * HP_MU); break;
        case HIT_SLIDER10: changeHealth(hpMultiplierNormal * HP_SLIDER10); break;
        case HIT_SLIDER30: changeHealth(hpMultiplierNormal * HP_SLIDER30); break;
        case HIT_SPINNERSPIN: changeHealth(hpMultiplierNormal * HP_SPINNERSPIN); break;
        case HIT_SPINNERBONUS: changeHealth(hpMultiplierNormal * HP_SPINNERBONUS); break;
        default: break;
    }
}

void Health::reset() {
    health = healthDisplay = healthUncapped = HP_MAX;
    hpDrainRate = 5.f;
    hpMultiplierNormal = hpMultiplierComboEnd = 1.f;
}

}  // namespace opsu
