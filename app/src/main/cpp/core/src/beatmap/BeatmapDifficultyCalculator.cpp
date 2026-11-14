#include "opsu/beatmap/BeatmapDifficultyCalculator.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "opsu/logging/Log.h"
#include "opsu/objects/curves/Curve.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaMath.h"

namespace opsu {

namespace {
/** The star scaling factor. */
constexpr double STAR_SCALING_FACTOR = 0.0675;
/** The scaling factor that favors extremes. */
constexpr double EXTREME_SCALING_FACTOR = 0.5;
/** The playfield width. */
constexpr float PLAYFIELD_WIDTH = 512.f;
/** Only the highest strain in each interval of this size (ms) is looked at. */
constexpr double STRAIN_STEP = 400;
/** The weighting of each strain value decays to 0.9 * its previous value. */
constexpr double DECAY_WEIGHT = 0.9;

/** Factor by how much speed / aim strain decays per second. */
constexpr double DECAY_BASE[2] = {0.3, 0.15};
/** Almost the normed diameter of a circle (104 osu pixel). That is -after- position transforming. */
constexpr double ALMOST_DIAMETER = 90;
/** Pseudo threshold values to distinguish between "singles" and "streams". */
constexpr double STREAM_SPACING_TRESHOLD = 110, SINGLE_SPACING_TRESHOLD = 125;
/** Scaling values for weightings to keep aim and speed difficulty in balance. */
constexpr double SPACING_WEIGHT_SCALING[2] = {1400, 26.25};
/** In milliseconds. The smaller the value, the more accurate sliders are approximated. */
constexpr int LAZY_SLIDER_STEP_LENGTH = 1;

constexpr int SPEED = BeatmapDifficultyCalculator::DIFFICULTY_SPEED;
constexpr int AIM = BeatmapDifficultyCalculator::DIFFICULTY_AIM;

/** Slider helper class to fill in some missing pieces needed in the strain calculations (Java: tpSlider). */
class TpSlider {
public:
    TpSlider(const HitObject& hitObject, float sliderMultiplier, float beatLength)
        : startTime_(hitObject.getTime()),
          sliderTime_(java::toInt(hitObject.getSliderTime(sliderMultiplier, beatLength))),
          curve_(hitObject.getSliderCurve(false)) {}

    /** The time duration of a slider segment, in milliseconds. */
    int getSegmentLength() const { return sliderTime_; }

    /** The coordinates of the slider at a given track position. */
    Vec2f getPositionAtTime(int time) const {
        // Java: `float t = (time - startTime) / sliderTime;` -- an INT division (kept as is).
        if (sliderTime_ == 0) throw std::domain_error("/ by zero");  // ArithmeticException
        float t = static_cast<float>((time - startTime_) / sliderTime_);
        float floor = static_cast<float>(std::floor(static_cast<double>(t)));
        t = (std::fmod(floor, 2.f) == 0) ? t - floor : floor + 1 - t;
        return curve_->pointAt(t);
    }

private:
    int startTime_;
    int sliderTime_;
    std::unique_ptr<Curve> curve_;
};

/** Returns the spacing weight for a distance. */
double spacingWeight(double distance, int type) {
    // Caution: The subjective values are strong with this one
    switch (type) {
        case SPEED: {
            double weight;
            if (distance > SINGLE_SPACING_TRESHOLD)
                weight = 2.5;
            else if (distance > STREAM_SPACING_TRESHOLD)
                weight = 1.6 + 0.9 * (distance - STREAM_SPACING_TRESHOLD) / (SINGLE_SPACING_TRESHOLD - STREAM_SPACING_TRESHOLD);
            else if (distance > ALMOST_DIAMETER)
                weight = 1.2 + 0.4 * (distance - ALMOST_DIAMETER) / (STREAM_SPACING_TRESHOLD - ALMOST_DIAMETER);
            else if (distance > ALMOST_DIAMETER / 2)
                weight = 0.95 + 0.25 * (distance - (ALMOST_DIAMETER / 2)) / (ALMOST_DIAMETER / 2);
            else
                weight = 0.95;
            return weight;
        }
        case AIM:
            return std::pow(distance, 0.99);
        default:
            return 0;  // Should never happen.
    }
}
}  // namespace

/** Hit object helper class for calculating strains (Java: tpHitObject). */
struct BeatmapDifficultyCalculator::TpHitObject {
    const HitObject* baseHitObject;
    double strains[2] = {1, 1};
    Vec2f normalizedStartPosition, normalizedEndPosition;
    float lazySliderLengthFirst = 0, lazySliderLengthSubsequent = 0;
    double clockRate;

    TpHitObject(const HitObject& base, float circleRadius, const Beatmap& beatmap, float beatLength, double clockRate_)
        : baseHitObject(&base), clockRate(clockRate_) {
        // We will scale everything by this factor, so we can assume a uniform CircleSize among beatmaps.
        float scalingFactor = (52.0f / circleRadius);
        normalizedStartPosition = Vec2f(base.getX(), base.getY()).scale(scalingFactor);

        // Calculate approximation of lazy movement on the slider
        if (base.isSlider()) {
            TpSlider slider(base, beatmap.sliderMultiplier, beatLength);

            // Not sure if this is correct, but here we do not need 100% exact values.
            float sliderFollowCircleRadius = circleRadius * 3;

            int segmentLength = slider.getSegmentLength();
            int segmentEndTime = base.getTime() + segmentLength;

            // For simplifying this step we use actual osu! coordinates and simply scale the length,
            // that we obtain by the ScalingFactor later
            Vec2f cursorPos(base.getX(), base.getY());

            // Actual computation of the first lazy curve
            for (int time = base.getTime() + LAZY_SLIDER_STEP_LENGTH; time < segmentEndTime; time += LAZY_SLIDER_STEP_LENGTH) {
                Vec2f difference = slider.getPositionAtTime(time).sub(cursorPos);
                float distance = difference.len();

                // Did we move away too far?
                if (distance > sliderFollowCircleRadius) {
                    // Yep, we need to move the cursor
                    difference.normalize();  // Obtain the direction of difference.
                    distance -= sliderFollowCircleRadius;
                    cursorPos.add(difference.cpy().scale(distance));  // move just as far as needed to stay in the follow circle
                    lazySliderLengthFirst += distance;
                }
            }

            lazySliderLengthFirst *= scalingFactor;

            // If we have an odd amount of repetitions the current position will be the end of the slider.
            // Note that this will -always- be triggered if SegmentCount <= 1, so normalizedEndPosition
            // is always initialized.
            if (base.getRepeatCount() % 2 == 1) normalizedEndPosition = cursorPos.cpy().scale(scalingFactor);

            // If we have more than one segment, then we also need to compute the length of subsequent
            // lazy curves. They are different from the first one, since the first one starts right
            // at the beginning of the slider.
            if (base.getRepeatCount() > 1) {
                // Use the next segment
                segmentEndTime += segmentLength;

                for (int time = segmentEndTime - segmentLength + LAZY_SLIDER_STEP_LENGTH; time < segmentEndTime; time += LAZY_SLIDER_STEP_LENGTH) {
                    Vec2f difference = slider.getPositionAtTime(time).sub(cursorPos);
                    float distance = difference.len();

                    if (distance > sliderFollowCircleRadius) {
                        difference.normalize();
                        distance -= sliderFollowCircleRadius;
                        cursorPos.add(difference.cpy().scale(distance));
                        lazySliderLengthSubsequent += distance;
                    }
                }

                lazySliderLengthSubsequent *= scalingFactor;

                // If we have an even amount of repetitions the current position will be the end of the slider
                if (base.getRepeatCount() % 2 == 0) normalizedEndPosition = cursorPos.cpy().scale(scalingFactor);
            }
        } else {
            // We have a normal HitCircle or a spinner
            normalizedEndPosition = normalizedStartPosition.cpy();
        }
    }

    double getStrain(int type) const { return strains[type]; }

    /** Returns the distance to another hit object (scaled by circle size). */
    double distanceTo(const TpHitObject& other) const {
        return (normalizedStartPosition.cpy().sub(other.normalizedEndPosition)).len();
    }

    /** Calculates the strain values given the previous hit object. */
    void calculateStrains(const TpHitObject& previousHitObject) {
        calculateSpecificStrain(previousHitObject, SPEED);
        calculateSpecificStrain(previousHitObject, AIM);
    }

    void calculateSpecificStrain(const TpHitObject& previousHitObject, int type) {
        double addition = 0;
        double timeElapsed = (baseHitObject->getTime() - previousHitObject.baseHitObject->getTime()) / clockRate;
        double decay = std::pow(DECAY_BASE[type], timeElapsed / 1000);

        if (baseHitObject->isSpinner()) {
            // Do nothing for spinners
        } else if (baseHitObject->isSlider()) {
            switch (type) {
                case SPEED:
                    // For speed strain we treat the whole slider as a single spacing entity,
                    // since "Speed" is about how hard it is to click buttons fast.
                    addition = spacingWeight(previousHitObject.lazySliderLengthFirst +
                                                 previousHitObject.lazySliderLengthSubsequent *
                                                     (java::max(previousHitObject.baseHitObject->getRepeatCount(), 1) - 1) +
                                                 distanceTo(previousHitObject), type) * SPACING_WEIGHT_SCALING[type];
                    break;
                case AIM:
                    // For Aim strain we treat each slider segment and the jump after the end of
                    // the slider as separate jumps, since movement-wise there is no difference
                    // to multiple jumps.
                    addition = (spacingWeight(previousHitObject.lazySliderLengthFirst, type) +
                                spacingWeight(previousHitObject.lazySliderLengthSubsequent, type) *
                                    (java::max(previousHitObject.baseHitObject->getRepeatCount(), 1) - 1) +
                                spacingWeight(distanceTo(previousHitObject), type)) * SPACING_WEIGHT_SCALING[type];
                    break;
            }
        } else if (baseHitObject->isCircle()) {
            addition = spacingWeight(distanceTo(previousHitObject), type) * SPACING_WEIGHT_SCALING[type];
        }

        // Scale addition by the time that elapsed. Filter out HitObjects that are too close to be
        // played anyway to avoid crazy values by division through close to zero.
        addition /= java::max(timeElapsed, 50.0);

        strains[type] = previousHitObject.strains[type] * decay + addition;
    }
};

BeatmapDifficultyCalculator::BeatmapDifficultyCalculator(const Beatmap& beatmap)
    : BeatmapDifficultyCalculator(beatmap, beatmap.circleSize, 1.f) {}

BeatmapDifficultyCalculator::BeatmapDifficultyCalculator(const Beatmap& beatmap, float circleSize, float clockRate)
    : beatmap_(beatmap), circleSize_(circleSize), clockRate_(static_cast<double>(clockRate > 0.f ? clockRate : 1.f)) {}

BeatmapDifficultyCalculator::~BeatmapDifficultyCalculator() = default;

void BeatmapDifficultyCalculator::calculate() {
    if (!beatmap_.objects || !beatmap_.timingPoints) {
        Log::error("Trying to calculate difficulty values for beatmap '" + beatmap_.toString(false) + "' with " +
                   (!beatmap_.objects ? "hit objects" : "timing points") + " not yet loaded.");
        return;
    }

    // Fill our custom tpHitObject class, that carries additional information
    // TODO (Java): apply hit object stacking algorithm?
    const std::vector<HitObject>& hitObjects = *beatmap_.objects;
    const std::vector<TimingPoint>& timingPoints = *beatmap_.timingPoints;
    tpHitObjects_.clear();
    tpHitObjects_.reserve(hitObjects.size());
    float circleRadius = (PLAYFIELD_WIDTH / 16.0f) * (1.0f - 0.7f * (circleSize_ - 5.0f) / 5.0f);
    size_t timingPointIndex = 0;
    float beatLengthBase = 1, beatLength = 1;
    if (!timingPoints.empty()) {
        const TimingPoint& timingPoint = timingPoints[0];
        if (!timingPoint.isInherited()) {
            beatLengthBase = beatLength = timingPoint.getBeatLength();
            timingPointIndex++;
        }
    }
    for (const HitObject& hitObject : hitObjects) {
        // pass beatLength to hit objects
        int hitObjectTime = hitObject.getTime();
        while (timingPointIndex < timingPoints.size()) {
            const TimingPoint& timingPoint = timingPoints[timingPointIndex];
            if (timingPoint.getTime() > hitObjectTime) break;
            if (!timingPoint.isInherited())
                beatLengthBase = beatLength = timingPoint.getBeatLength();
            else
                beatLength = beatLengthBase * timingPoint.getSliderMultiplier();
            timingPointIndex++;
        }

        tpHitObjects_.emplace_back(hitObject, circleRadius, beatmap_, beatLength, clockRate_);
    }

    if (!calculateStrainValues()) {
        Log::error("Could not compute strain values. Aborting difficulty calculation.");
        return;
    }

    // OverallDifficulty is not considered in this algorithm and neither is HpDrainRate.
    // (see the long rationale in the Java original)
    difficulties_[SPEED] = calculateDifficulty(SPEED);
    difficulties_[AIM] = calculateDifficulty(AIM);

    // Forge a star rating from the difficulties: square root, then scaled.
    stars_[SPEED] = std::sqrt(difficulties_[SPEED]) * STAR_SCALING_FACTOR;
    stars_[AIM] = std::sqrt(difficulties_[AIM]) * STAR_SCALING_FACTOR;

    // A map with high speed and low aim (or vice versa) difficulty is harder than a map with mediocre
    // difficulty in both, so use a scaling that favors extremes.
    starRating_ = stars_[SPEED] + stars_[AIM] + std::fabs(stars_[SPEED] - stars_[AIM]) * EXTREME_SCALING_FACTOR;
}

bool BeatmapDifficultyCalculator::calculateStrainValues() {
    // Traverse hitObjects in pairs to calculate the strain value of NextHitObject from
    // the strain value of CurrentHitObject and environment.
    if (tpHitObjects_.empty()) {
        Log::warn("Can not compute difficulty of empty beatmap.");
        return false;
    }

    // First hitObject starts at strain 1. 1 is the default for strain values, so we don't need to set it here.
    for (size_t index = 1; index < tpHitObjects_.size(); index++)
        tpHitObjects_[index].calculateStrains(tpHitObjects_[index - 1]);

    return true;
}

double BeatmapDifficultyCalculator::calculateDifficulty(int type) const {
    // Find the highest strain value within each strain step
    std::vector<double> highestStrains;
    double intervalEndTime = STRAIN_STEP;
    double maximumStrain = 0;  // We need to keep track of the maximum strain in the current interval

    const TpHitObject* previousHitObject = nullptr;
    for (const TpHitObject& hitObject : tpHitObjects_) {
        // While we are beyond the current interval push the currently available maximum to our strain list
        while (hitObject.baseHitObject->getTime() / clockRate_ > intervalEndTime) {
            highestStrains.push_back(maximumStrain);

            // The maximum strain of the next interval is not zero by default! We need to take the last
            // hitObject we encountered, take its strain and apply the decay until the beginning of the next interval.
            if (previousHitObject == nullptr)
                maximumStrain = 0;
            else {
                double decay = std::pow(DECAY_BASE[type], (intervalEndTime - previousHitObject->baseHitObject->getTime() / clockRate_) / 1000);
                maximumStrain = previousHitObject->getStrain(type) * decay;
            }

            // Go to the next time interval
            intervalEndTime += STRAIN_STEP;
        }

        // Obtain maximum strain
        if (hitObject.getStrain(type) > maximumStrain) maximumStrain = hitObject.getStrain(type);

        previousHitObject = &hitObject;
    }

    // Build the weighted sum over the highest strains for each interval
    double difficulty = 0;
    double weight = 1;
    // Collections.sort(list, Collections.reverseOrder()): highest to lowest, Double.compare ordering
    std::stable_sort(highestStrains.begin(), highestStrains.end(), [](double a, double b) { return java::compare(b, a) < 0; });
    for (double strain : highestStrains) {
        difficulty += weight * strain;
        weight *= DECAY_WEIGHT;
    }

    return difficulty;
}

}  // namespace opsu
