#include "opsu/beatmap/TimingPoint.h"
#include <stdexcept>
#include "opsu/Utils.h"
#include "opsu/logging/Log.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

const TimingPoint TimingPoint::DEFAULT("0,1000.0,4,1,0,100,0,0");

TimingPoint::TimingPoint(const std::string& line) {
    // offset,msPerBeat,meter,sampleType,sampleSet,volume,inherited,kiai
    std::vector<std::string> tokens = java::split(line, ',');
    try {
        time_ = java::toInt(java::parseFloat(tokens.at(0)));  // rare float
        meter_ = java::parseInt(tokens.at(2));
        sampleType_ = java::parseByte(tokens.at(3));
        sampleTypeCustom_ = java::parseByte(tokens.at(4));
        sampleVolume_ = java::parseInt(tokens.at(5));
        // (Java kept 'inherited' derived from the sign of tokens[1]; see kww note in the original.)
        if (tokens.size() >= 7) kiai_ = Utils::parseBoolean(tokens.at(7));
    } catch (const std::out_of_range&) {  // ArrayIndexOutOfBoundsException
        Log::debug("Error parsing timing point: '" + line + "'");
    }

    // tokens[1] is either beatLength (positive) or velocity (negative)
    float beatLength = java::parseFloat(tokens.at(1));
    if (beatLength > 0)
        beatLength_ = beatLength;
    else {
        velocity_ = java::toInt(beatLength);
        inherited_ = true;
    }
}

float TimingPoint::getSliderMultiplier() const { return Utils::clamp(-velocity_, 10, 1000) / 100.f; }

std::string TimingPoint::toString() const {
    using std::to_string;
    std::string s = to_string(time_) + ",";
    s += inherited_ ? to_string(velocity_) : java::formatGeneral(beatLength_);
    s += "," + to_string(meter_) + "," + to_string(static_cast<int>(sampleType_)) + "," +
         to_string(static_cast<int>(sampleTypeCustom_)) + "," + to_string(sampleVolume_) + "," +
         (inherited_ ? "1" : "0") + "," + (kiai_ ? "1" : "0");
    return s;
}

}  // namespace opsu
