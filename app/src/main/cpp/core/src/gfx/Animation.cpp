#include "opsu/gfx/Animation.h"

#include <chrono>
#include <stdexcept>

namespace opsu {

namespace {
int64_t (*g_clock)() = nullptr;
int64_t nowMs() {
    if (g_clock) return g_clock();
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
}  // namespace

void Animation::setClockForTests(int64_t (*clock)()) { g_clock = clock; }

Animation::Animation(const SpriteSheet& sheet, int durationMs) : lastUpdate_(nowMs()) {
    const auto& img = sheet.getImage();
    if (!img || sheet.getTileWidth() <= 0 || sheet.getTileHeight() <= 0) return;
    for (int y = 0; y < img->getHeight(); y += sheet.getTileHeight())
        for (int x = 0; x < img->getWidth(); x += sheet.getTileWidth())
            addFrame(std::make_shared<Image>(*img, x, y, sheet.getTileWidth(), sheet.getTileHeight()), durationMs);
}

Animation::Animation(const std::vector<std::shared_ptr<Image>>& frames, int durationMs) : lastUpdate_(nowMs()) {
    for (const auto& f : frames) addFrame(f, durationMs);
}

void Animation::addFrame(std::shared_ptr<Image> img, int durationMs) { frames_.push_back(Frame{std::move(img), durationMs}); }

int Animation::getWidth() const { return frames_.empty() ? 0 : frames_[0].img->getWidth(); }
int Animation::getHeight() const { return frames_.empty() ? 0 : frames_[0].img->getHeight(); }

Image Animation::getScaledCopy(float, float) const { throw std::logic_error("Animation.getScaledCopy: Not Implemented"); }
Image Animation::getScaledCopy(float) const { throw std::logic_error("Animation.getScaledCopy: Not Implemented"); }

TextureRegion Animation::getTextureRegion() const {
    if (frames_.empty()) return TextureRegion();
    return frames_[static_cast<size_t>(curFrame_)].img->getTextureRegion();
}

void Animation::updateNoDraw() {
    if (frames_.empty()) return;
    while (nowMs() - lastUpdate_ > frames_[static_cast<size_t>(curFrame_)].duration) {
        lastUpdate_ = nowMs();
        curFrame_ = (curFrame_ + 1) % getFrameCount();
    }
}

}  // namespace opsu
