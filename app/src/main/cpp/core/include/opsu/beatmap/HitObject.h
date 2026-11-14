#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "opsu/objects/curves/Curve.h"

namespace opsu {

/**
 * A parsed hit object. Port of itdelatrisu.opsu.beatmap.HitObject.
 *
 * Java null arrays/strings are represented as empty vectors/strings (output-equivalent).
 * The constructor throws java::NumberFormatException / std::out_of_range on malformed
 * lines, where Java threw NumberFormatException / ArrayIndexOutOfBoundsException.
 */
class HitObject {
public:
    /** Hit object types (bits). */
    static constexpr int TYPE_CIRCLE = 1, TYPE_SLIDER = 2, TYPE_NEWCOMBO = 4 /* not an object */, TYPE_SPINNER = 8;
    /** Hit sound types (bits). */
    static constexpr int8_t SOUND_NORMAL = 0, SOUND_WHISTLE = 2, SOUND_FINISH = 4, SOUND_CLAP = 8;
    /** Slider curve types. */
    static constexpr char SLIDER_CATMULL = 'C', SLIDER_BEZIER = 'B', SLIDER_LINEAR = 'L', SLIDER_PERFECT_CURVE = 'P';

    /**
     * Initializes the container-dependent statics.
     * @param playfieldScale Options.getPlayfieldScale() in Java (PLAYFIELDSCALE / 10; default 1.0 on
     *        desktop, 1.6 on small screens). Pass it explicitly until Options is ported.
     */
    static void init(int width, int height, float playfieldScale = 1.0f);

    static float getStackOffset() { return stackOffset_; }
    static void setStackOffset(float offset) { stackOffset_ = offset; }
    static float getXMultiplier() { return xMultiplier_; }
    static float getYMultiplier() { return yMultiplier_; }
    static int getXOffset() { return xOffset_; }
    static int getYOffset() { return yOffset_; }

    explicit HitObject(const std::string& line);

    float getX() const { return x_; }
    float getY() const { return y_; }
    float getScaledX() const;
    float getScaledY() const;
    int getTime() const { return time_; }
    int getType() const { return type_; }
    std::string getTypeName() const;
    int16_t getHitSoundType() const { return hitSound_; }
    int16_t getEdgeHitSoundType(int index) const;
    char getSliderType() const { return sliderType_; }
    const std::vector<float>& getSliderX() const { return sliderX_; }
    const std::vector<float>& getSliderY() const { return sliderY_; }
    std::vector<float> getScaledSliderX() const;
    std::vector<float> getScaledSliderY() const;
    int getRepeatCount() const { return repeat_; }
    float getPixelLength() const { return pixelLength_; }
    float getSliderTime(float sliderMultiplier, float beatLength) const {
        return beatLength * (pixelLength_ / sliderMultiplier) / 100.f;
    }
    /** Returns a new slider Curve. May throw std::runtime_error (degenerate curves). */
    std::unique_ptr<Curve> getSliderCurve(bool scaled = true) const;
    int getEndTime() const { return endTime_; }

    void setComboIndex(int i) { comboIndex_ = i; }
    int getComboIndex() const { return comboIndex_; }
    void setComboNumber(int n) { comboNumber_ = n; }
    int getComboNumber() const { return comboNumber_; }

    bool isCircle() const { return (type_ & TYPE_CIRCLE) > 0; }
    bool isSlider() const { return (type_ & TYPE_SLIDER) > 0; }
    bool isSpinner() const { return (type_ & TYPE_SPINNER) > 0; }
    bool isNewCombo() const { return (type_ & TYPE_NEWCOMBO) > 0; }
    int getComboSkip() const { return type_ >> TYPE_NEWCOMBO; }

    int8_t getSampleSet(int index) const;
    int8_t getAdditionSampleSet(int index) const;
    int8_t getObjectSampleSet() const { return hasAddition_ ? addition_[0] : 0; }
    int8_t getObjectAdditionSampleSet() const { return hasAddition_ ? addition_[1] : 0; }
    int8_t getCustomSampleIndex() const { return additionCustomSampleIndex_; }
    int getHitSoundVolume() const { return additionHitSoundVolume_; }
    /** The hit sound file (addition); empty if none (Java: null). */
    const std::string& getHitSoundFile() const { return additionHitSound_; }

    void setStack(int stack) { stack_ = stack; }
    int getStack() const { return stack_; }

    std::string toString() const;

private:
    static constexpr int MAX_X = 512, MAX_Y = 384;
    static float xMultiplier_, yMultiplier_;
    static int xOffset_, yOffset_;
    static int containerHeight_;
    static float stackOffset_;

    float x_ = 0, y_ = 0;
    int time_ = 0;
    int type_ = 0;
    int16_t hitSound_ = 0;
    bool hasAddition_ = false;
    std::array<int8_t, 2> addition_{{0, 0}};
    int8_t additionCustomSampleIndex_ = 0;
    int additionHitSoundVolume_ = 0;
    std::string additionHitSound_;
    char sliderType_ = 0;
    std::vector<float> sliderX_, sliderY_;
    int repeat_ = 0;
    float pixelLength_ = 0;
    int endTime_ = 0;
    std::vector<int16_t> edgeHitSound_;
    std::vector<std::array<int8_t, 2>> edgeAddition_;
    int comboIndex_ = 0;
    int comboNumber_ = 0;
    int stack_ = 0;
};

}  // namespace opsu
