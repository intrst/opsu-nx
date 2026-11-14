#include "opsu/beatmap/BeatmapHPDropRateCalculator.h"
#include <cmath>
#include <stdexcept>
#include "opsu/GameData.h"
#include "opsu/Utils.h"
#include "opsu/beatmap/Health.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

void BeatmapHPDropRateCalculator::calculate() {
    // Same order as the Java would fail in: objects[0] first (null -> NPE, empty -> AIOOBE), then timingPoints.
    if (!beatmap_.objects)
        throw std::logic_error("BeatmapHPDropRateCalculator: hit objects must be loaded");
    const std::vector<HitObject>& objects = *beatmap_.objects;
    if (objects.empty())
        throw std::out_of_range("BeatmapHPDropRateCalculator: beatmap has no hit objects");
    if (!beatmap_.timingPoints)
        throw std::logic_error("BeatmapHPDropRateCalculator: timing points must be loaded");
    const std::vector<TimingPoint>& timingPoints = *beatmap_.timingPoints;

    // NOTE: literal types are kept exactly as in the Java (e.g. `*= 1.03` is a double multiply that is
    // narrowed back to float, `*= 0.96f` is a float multiply) so the results match bit for bit.
    float lowestHpEver = Utils::mapDifficultyRange(hpDrainRate_, 195, 160, 60);
    float lowestHpComboEnd = Utils::mapDifficultyRange(hpDrainRate_, 198, 170, 80);
    float lowestHpEnd = Utils::mapDifficultyRange(hpDrainRate_, 198, 180, 80);
    float hpRecoveryAvailable = Utils::mapDifficultyRange(hpDrainRate_, 8, 4, 0);
    int approachTime = java::toInt(Utils::mapDifficultyRange(overallDifficulty_, 1800, 1200, 450));

    float testDrop = 0.05f;
    Health health;
    hpMultiplierNormal_ = hpMultiplierComboEnd_ = 1.0f;
    const int n = static_cast<int>(objects.size());

    while (true) {
        health.reset();
        health.setModifiers(hpDrainRate_, hpMultiplierNormal_, hpMultiplierComboEnd_);
        double lowestHp = health.getRawHealth();
        int lastTime = objects[0].getTime() - approachTime;
        int comboTooLowCount = 0;
        bool fail = false;

        int timingPointIndex = 0;
        float beatLengthBase = 1.f, beatLength = 1.f;

        for (int i = 0; i < n; i++) {
            const HitObject& hitObject = objects[i];

            // breaks
            int breakTime = 0;
            if (beatmap_.breaks) {
                const std::vector<int>& breaks = *beatmap_.breaks;
                for (int j = 0; j < static_cast<int>(breaks.size()); j += 2) {
                    int breakStart = breaks.at(j), breakEnd = breaks.at(j + 1);
                    if (breakStart >= lastTime && breakEnd <= hitObject.getTime()) {
                        breakTime = breakEnd - breakStart;
                        break;
                    }
                }
            }
            health.changeHealth(-testDrop * (hitObject.getTime() - lastTime - breakTime));

            // pass beatLength to hit objects
            int hitObjectTime = hitObject.getTime();
            while (timingPointIndex < static_cast<int>(timingPoints.size())) {
                const TimingPoint& timingPoint = timingPoints[timingPointIndex];
                if (timingPoint.getTime() > hitObjectTime)
                    break;
                if (!timingPoint.isInherited())
                    beatLengthBase = beatLength = timingPoint.getBeatLength();
                else
                    beatLength = beatLengthBase * timingPoint.getSliderMultiplier();
                timingPointIndex++;
            }

            // compute end time
            int endTime;
            if (hitObject.isCircle())
                endTime = hitObject.getTime();
            else if (hitObject.isSlider()) {
                float sliderTime = hitObject.getSliderTime(beatmap_.sliderMultiplier, beatLength);
                float sliderTimeTotal = sliderTime * hitObject.getRepeatCount();
                endTime = hitObject.getTime() + java::toInt(sliderTimeTotal);
            } else
                endTime = hitObject.getEndTime();
            lastTime = endTime;

            if (health.getRawHealth() < lowestHp)
                lowestHp = health.getRawHealth();
            if (health.getRawHealth() <= lowestHpEver) {
                fail = true;
                testDrop *= 0.96f;
                break;
            }
            health.changeHealth(-testDrop * (endTime - hitObject.getTime()));

            // hit objects
            if (hitObject.isSlider()) {
                float tickLengthDiv =
                    100.f * beatmap_.sliderMultiplier / beatmap_.sliderTickRate / (beatLength / beatLengthBase);
                int tickCount = java::toInt(std::ceil(static_cast<double>(hitObject.getPixelLength() / tickLengthDiv))) - 1;
                for (int j = 0; j < hitObject.getRepeatCount(); j++)
                    health.changeHealthForHit(GameData::HIT_SLIDER30);
                for (int j = 0; j < tickCount * hitObject.getRepeatCount(); j++)
                    health.changeHealthForHit(GameData::HIT_SLIDER10);
            } else if (hitObject.isSpinner()) {
                float spinsPerMinute = 100 + (beatmap_.overallDifficulty * 15);
                int rotationsNeeded =
                    java::toInt(spinsPerMinute * (hitObject.getEndTime() - hitObject.getTime()) / 60000.f);
                for (int j = 0; j < rotationsNeeded; j++)
                    health.changeHealthForHit(GameData::HIT_SPINNERSPIN);
            }
            health.changeHealthForHit(GameData::HIT_300);
            if (i == n - 1 || objects[i + 1].isNewCombo()) {
                health.changeHealthForHit(GameData::HIT_300G);
                if (health.getRawHealth() < lowestHpComboEnd) {
                    if (++comboTooLowCount > 2) {
                        fail = true;
                        hpMultiplierNormal_ *= 1.03;
                        hpMultiplierComboEnd_ *= 1.07;
                        break;
                    }
                }
            }
        }

        if (!fail && health.getRawHealth() < lowestHpEnd) {
            fail = true;
            testDrop *= 0.94f;
            hpMultiplierNormal_ *= 1.01;
            hpMultiplierComboEnd_ *= 1.01;
        }

        double recovery = (health.getUncappedRawHealth() - Health::HP_MAX) / n;
        if (!fail && recovery < hpRecoveryAvailable) {
            fail = true;
            testDrop *= 0.96;
            hpMultiplierNormal_ *= 1.01;
            hpMultiplierComboEnd_ *= 1.02;
        }

        if (fail)
            continue;

        hpDropRate_ = testDrop;
        break;
    }
}

}  // namespace opsu
