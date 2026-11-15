#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "opsu/gfx/Image.h"
#include "opsu/gfx/SpriteSheet.h"

namespace opsu {

/**
 * fluddokt.opsu.fake.Animation: an Image made of frames with a duration each. Drawing uses the current frame; the frame
 * only advances in updateNoDraw() (at most one frame per call, like the Java). Not ported: Animation(String filename)
 * (it throws "Not Implemented" in the Java); getScaledCopy throws std::logic_error like the Java Error.
 */
class Animation : public Image {
public:
    Animation() = default;
    Animation(const SpriteSheet& sheet, int durationMs);
    Animation(const std::vector<std::shared_ptr<Image>>& frames, int durationMs);

    int getWidth() const override;
    int getHeight() const override;
    Image getScaledCopy(float w, float h) const override;
    Image getScaledCopy(float f) const override;
    TextureRegion getTextureRegion() const override;

    int getFrameCount() const { return static_cast<int>(frames_.size()); }
    std::shared_ptr<Image> getImage(int i) const { return frames_.at(static_cast<size_t>(i)).img; }
    std::shared_ptr<Image> getCurrentFrame() const { return getImage(curFrame_); }
    void addFrame(std::shared_ptr<Image> img, int durationMs);

    /** Advances to the next frame if the current one has been shown longer than its duration. */
    void updateNoDraw();

    /** Milliseconds clock; replaceable in tests. */
    static void setClockForTests(int64_t (*clock)());

private:
    struct Frame {
        std::shared_ptr<Image> img;
        int duration;
    };
    std::vector<Frame> frames_;
    int curFrame_ = 0;
    int64_t lastUpdate_ = 0;
};

}  // namespace opsu
