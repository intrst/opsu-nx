#pragma once

namespace opsu {

/** Health bar. Port of itdelatrisu.opsu.beatmap.Health (peppy, ppy/osu-iPhone). */
class Health {
public:
    static constexpr float HP_MAX = 200.f;

    Health() { reset(); }

    void setModifiers(float hpDrainRate, float hpMultiplierNormal, float hpMultiplierComboEnd);
    /** Updates displayed health by a delta interval (ms). */
    void update(int delta);
    float getHealth() const { return health / HP_MAX * 100.f; }
    float getHealthDisplay() const { return healthDisplay / HP_MAX * 100.f; }
    float getRawHealth() const { return health; }
    float getUncappedRawHealth() const { return healthUncapped; }
    void setHealth(float value) { health = healthUncapped = value; }
    void changeHealth(float value);
    /** Changes health for a hit (GameData::HIT_* constants). */
    void changeHealthForHit(int type);
    void reset();

private:
    float health = 0, healthDisplay = 0, healthUncapped = 0;
    float hpDrainRate = 5.f;
    float hpMultiplierNormal = 1.f;
    float hpMultiplierComboEnd = 1.f;
};

}  // namespace opsu
