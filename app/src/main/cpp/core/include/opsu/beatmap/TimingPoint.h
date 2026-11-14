#pragma once
#include <cstdint>
#include <string>

namespace opsu {

/** A timing point. Port of itdelatrisu.opsu.beatmap.TimingPoint. */
class TimingPoint {
public:
    static const TimingPoint DEFAULT;

    /** @throws java::NumberFormatException / std::out_of_range on malformed lines (like Java). */
    explicit TimingPoint(const std::string& line);

    int getTime() const { return time_; }
    float getBeatLength() const { return beatLength_; }
    float getSliderMultiplier() const;
    int getMeter() const { return meter_; }
    int8_t getSampleType() const { return sampleType_; }
    int8_t getSampleTypeCustom() const { return sampleTypeCustom_; }
    float getSampleVolume() const { return sampleVolume_ / 100.f; }
    bool isInherited() const { return inherited_; }
    bool isKiaiTimeActive() const { return kiai_; }

    std::string toString() const;

private:
    static constexpr float default_beat_length = 60000.0f / 60.0f;

    int time_ = 0;
    float beatLength_ = default_beat_length;
    int velocity_ = 0;
    int meter_ = 4;
    int8_t sampleType_ = 1;
    int8_t sampleTypeCustom_ = 0;
    int sampleVolume_ = 100;
    bool inherited_ = false;
    bool kiai_ = false;
};

}  // namespace opsu
