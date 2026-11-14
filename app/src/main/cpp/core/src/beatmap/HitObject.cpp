#include "opsu/beatmap/HitObject.h"
#include <algorithm>
#include <cmath>
#include "opsu/GameMod.h"
#include "opsu/Utils.h"
#include "opsu/objects/curves/CatmullCurve.h"
#include "opsu/objects/curves/CircumscribedCircle.h"
#include "opsu/objects/curves/LinearBezier.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

float HitObject::xMultiplier_ = 0, HitObject::yMultiplier_ = 0;
int HitObject::xOffset_ = 0, HitObject::yOffset_ = 0;
int HitObject::containerHeight_ = 0;
float HitObject::stackOffset_ = 0;

void HitObject::init(int width, int height, float playfieldScale) {
    containerHeight_ = height;
    int swidth = width;
    int sheight = height;
    if (swidth * 3 > sheight * 4)
        swidth = sheight * 4 / 3;
    else
        sheight = swidth * 3 / 4;
    // 640 = 1  512 = 2
    // 640 - (640-512) * clamp(x-1,0,1)
    float scale = Utils::clamp(playfieldScale - 1, 0.0f, 1.0f);
    xMultiplier_ = static_cast<float>(swidth) / (640.0f - (640 - MAX_X) * scale);
    yMultiplier_ = static_cast<float>(sheight) / (480.0f - (480 - MAX_Y) * scale);

    xOffset_ = java::toInt(static_cast<float>(width) - MAX_X * xMultiplier_) / 2;
    yOffset_ = java::toInt(static_cast<float>(height) - MAX_Y * yMultiplier_) / 2;
}

HitObject::HitObject(const std::string& line) {
    /*
     * Circles:  x,y,time,type,hitSound,addition
     * Sliders:  x,y,time,type,hitSound,sliderType|curveX:curveY|...,repeat,pixelLength,edgeHitsound,edgeAddition,addition
     * Spinners: x,y,time,type,hitSound,endTime,addition
     * 'addition' -> sampl:add:cust:vol:hitsound (optional)
     */
    std::vector<std::string> tokens = java::split(line, ',');

    // common fields
    x_ = java::parseFloat(tokens.at(0));
    y_ = java::parseFloat(tokens.at(1));
    time_ = java::parseInt(tokens.at(2));
    type_ = java::parseInt(tokens.at(3));
    hitSound_ = java::parseByte(tokens.at(4));

    // type-specific fields
    size_t additionIndex;
    if ((type_ & TYPE_CIRCLE) > 0)
        additionIndex = 5;
    else if ((type_ & TYPE_SLIDER) > 0) {
        additionIndex = 10;

        // slider curve type and coordinates
        std::vector<std::string> sliderTokens = java::split(tokens.at(5), '|');
        sliderType_ = sliderTokens.at(0).at(0);
        sliderX_.assign(sliderTokens.size() - 1, 0.f);
        sliderY_.assign(sliderTokens.size() - 1, 0.f);
        for (size_t j = 1; j < sliderTokens.size(); j++) {
            std::vector<std::string> sliderXY = java::split(sliderTokens[j], ':');
            sliderX_[j - 1] = static_cast<float>(java::parseInt(sliderXY.at(0)));
            sliderY_[j - 1] = static_cast<float>(java::parseInt(sliderXY.at(1)));
        }
        repeat_ = java::parseInt(tokens.at(6));
        pixelLength_ = java::parseFloat(tokens.at(7));
        if (tokens.size() > 8) {
            std::vector<std::string> edgeHitSoundTokens = java::split(tokens[8], '|');
            edgeHitSound_.assign(edgeHitSoundTokens.size(), 0);
            for (size_t j = 0; j < edgeHitSoundTokens.size(); j++)
                edgeHitSound_[j] = java::parseShort(edgeHitSoundTokens[j]);
        }
        if (tokens.size() > 9) {
            std::vector<std::string> edgeAdditionTokens = java::split(tokens[9], '|');
            edgeAddition_.assign(edgeAdditionTokens.size(), {{0, 0}});
            for (size_t j = 0; j < edgeAdditionTokens.size(); j++) {
                std::vector<std::string> t = java::split(edgeAdditionTokens[j], ':');
                edgeAddition_[j][0] = java::parseByte(t.at(0));
                edgeAddition_[j][1] = java::parseByte(t.at(1));
            }
        }
    } else {  // spinner
        additionIndex = 6;

        // some 'endTime' fields contain a ':' character (?)
        size_t index = tokens.at(5).find(':');
        if (index != std::string::npos) tokens[5] = tokens[5].substr(0, index);
        endTime_ = java::parseInt(tokens[5]);
    }

    // addition
    if (tokens.size() > additionIndex) {
        std::vector<std::string> additionTokens = java::split(tokens[additionIndex], ':');
        if (additionTokens.size() > 1) {
            hasAddition_ = true;
            addition_[0] = java::parseByte(additionTokens[0]);
            addition_[1] = java::parseByte(additionTokens[1]);
        }
        if (additionTokens.size() > 2) additionCustomSampleIndex_ = java::parseByte(additionTokens[2]);
        if (additionTokens.size() > 3) additionHitSoundVolume_ = java::parseInt(additionTokens[3]);
        if (additionTokens.size() > 4) additionHitSound_ = additionTokens[4];
    }
}

float HitObject::getScaledX() const {
    return (x_ - static_cast<float>(stack_) * stackOffset_) * xMultiplier_ + static_cast<float>(xOffset_);
}

float HitObject::getScaledY() const {
    float st = static_cast<float>(stack_) * stackOffset_;
    if (GameMod::HARD_ROCK.isActive())
        return static_cast<float>(containerHeight_) - ((y_ + st) * yMultiplier_ + static_cast<float>(yOffset_));
    return (y_ - st) * yMultiplier_ + static_cast<float>(yOffset_);
}

std::string HitObject::getTypeName() const {
    if (isCircle()) return "circle";
    if (isSlider()) return "slider";
    if (isSpinner()) return "spinner";
    return "unknown object type";
}

int16_t HitObject::getEdgeHitSoundType(int index) const {
    if (!edgeHitSound_.empty())
        return edgeHitSound_[static_cast<size_t>(std::max(0, std::min(index, static_cast<int>(edgeHitSound_.size()) - 1)))];
    return hitSound_;
}

std::vector<float> HitObject::getScaledSliderX() const {
    std::vector<float> x(sliderX_.size());
    for (size_t i = 0; i < x.size(); i++)
        x[i] = (sliderX_[i] - static_cast<float>(stack_) * stackOffset_) * xMultiplier_ + static_cast<float>(xOffset_);
    return x;
}

std::vector<float> HitObject::getScaledSliderY() const {
    std::vector<float> y(sliderY_.size());
    float st = static_cast<float>(stack_) * stackOffset_;
    if (GameMod::HARD_ROCK.isActive()) {
        for (size_t i = 0; i < y.size(); i++)
            y[i] = static_cast<float>(containerHeight_) - ((sliderY_[i] + st) * yMultiplier_ + static_cast<float>(yOffset_));
    } else {
        for (size_t i = 0; i < y.size(); i++) y[i] = (sliderY_[i] - st) * yMultiplier_ + static_cast<float>(yOffset_);
    }
    return y;
}

std::unique_ptr<Curve> HitObject::getSliderCurve(bool scaled) const {
    if (sliderType_ == SLIDER_PERFECT_CURVE && sliderX_.size() == 2) {
        Vec2f nora = Vec2f(sliderX_[0] - x_, sliderY_[0] - y_).nor();
        Vec2f norb = Vec2f(sliderX_[0] - sliderX_[1], sliderY_[0] - sliderY_[1]).nor();
        if (std::fabs(norb.x * nora.y - norb.y * nora.x) < 0.00001f)
            return std::make_unique<LinearBezier>(*this, false, scaled);  // vectors parallel, use linear bezier
        return std::make_unique<CircumscribedCircle>(*this, scaled);
    } else if (sliderType_ == SLIDER_CATMULL)
        return std::make_unique<CatmullCurve>(*this, scaled);
    return std::make_unique<LinearBezier>(*this, sliderType_ == SLIDER_LINEAR, scaled);
}

int8_t HitObject::getSampleSet(int index) const {
    if (!edgeAddition_.empty())
        return edgeAddition_[static_cast<size_t>(std::max(0, std::min(index, static_cast<int>(edgeAddition_.size()) - 1)))][0];
    if (hasAddition_) return addition_[0];
    return 0;
}

int8_t HitObject::getAdditionSampleSet(int index) const {
    if (!edgeAddition_.empty())
        return edgeAddition_[static_cast<size_t>(std::max(0, std::min(index, static_cast<int>(edgeAddition_.size()) - 1)))][1];
    if (hasAddition_) return addition_[1];
    return 0;
}

std::string HitObject::toString() const {
    using std::to_string;
    auto nf = [](double v) { return java::formatTrimmed(v, 5); };
    std::string sb;

    // common fields
    sb += nf(x_) + ",";
    sb += nf(y_) + ",";
    sb += to_string(time_) + ",";
    sb += to_string(type_) + ",";
    sb += to_string(hitSound_) + ",";

    // type-specific fields
    if (isCircle()) {
    } else if (isSlider()) {
        sb += sliderType_;
        sb += '|';
        for (size_t i = 0; i < sliderX_.size(); i++) {
            sb += nf(sliderX_[i]) + ":";
            sb += nf(sliderY_[i]) + "|";
        }
        sb.back() = ',';
        sb += to_string(repeat_) + ",";
        sb += java::floatToString(pixelLength_) + ",";
        if (!edgeHitSound_.empty()) {
            for (int16_t e : edgeHitSound_) sb += to_string(e) + "|";
            sb.back() = ',';
        }
        if (!edgeAddition_.empty()) {
            for (const auto& e : edgeAddition_) sb += to_string(e[0]) + ":" + to_string(e[1]) + "|";
            sb.back() = ',';
        }
    } else if (isSpinner()) {
        sb += to_string(endTime_) + ",";
    }

    // addition
    if (hasAddition_) {
        for (int8_t a : addition_) sb += to_string(a) + ":";
        sb += to_string(additionCustomSampleIndex_) + ":";
        sb += to_string(additionHitSoundVolume_) + ":";
        sb += additionHitSound_;
    } else
        sb.pop_back();

    return sb;
}

}  // namespace opsu
