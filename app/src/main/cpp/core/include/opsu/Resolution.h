#pragma once

namespace opsu {

/**
 * A screen size in pixels. Port of fluddokt.opsu.fake.Resolution.
 *
 * Not to be confused with the private `Options.Resolution` enum of the Java options
 * (the list of selectable window sizes); that one comes with Options.
 */
class Resolution {
public:
    Resolution(int width, int height) : width(width), height(height) {}

    int getHeight() const { return height; }
    int getWidth() const { return width; }

private:
    int width, height;
};

}  // namespace opsu
