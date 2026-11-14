#pragma once
#include <memory>

#include "opsu/gfx/Image.h"

namespace opsu {

/** fluddokt.opsu.fake.SpriteSheet: an image cut into width x height tiles (left to right, top to bottom). */
class SpriteSheet {
public:
    SpriteSheet(std::shared_ptr<Image> image, int width, int height) : image_(std::move(image)), width_(width), height_(height) {}
    const std::shared_ptr<Image>& getImage() const { return image_; }
    int getTileWidth() const { return width_; }
    int getTileHeight() const { return height_; }

private:
    std::shared_ptr<Image> image_;
    int width_, height_;
};

}  // namespace opsu
