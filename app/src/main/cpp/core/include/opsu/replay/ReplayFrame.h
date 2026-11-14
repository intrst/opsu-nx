#pragma once
#include <string>

namespace opsu {

/** A single replay frame. Port of itdelatrisu.opsu.replay.ReplayFrame (by smoogipooo). */
class ReplayFrame {
public:
    static constexpr int KEY_NONE = 0, KEY_M1 = (1 << 0), KEY_M2 = (1 << 1),
                         KEY_K1 = (1 << 2) | (1 << 0), KEY_K2 = (1 << 3) | (1 << 1);

    /** Multitouch bits: one independent bit per overlay key (see the Java doc). */
    static constexpr int KEY_TOUCH_M1 = (1 << 8), KEY_TOUCH_M2 = (1 << 9), KEY_TOUCH_K1 = (1 << 10),
                         KEY_TOUCH_K2 = (1 << 11),
                         KEY_TOUCH_MASK = KEY_TOUCH_M1 | KEY_TOUCH_M2 | KEY_TOUCH_K1 | KEY_TOUCH_K2;

    static ReplayFrame getStartFrame(int t) { return ReplayFrame(t, t, 256, -500, 0); }

    ReplayFrame(int timeDiff, int time, float x, float y, int keys)
        : timeDiff_(timeDiff), time_(time), x_(x), y_(y), keys_(keys) {}

    int getTime() const { return time_; }
    int getTimeDiff() const { return timeDiff_; }
    void setTimeDiff(int diff) { timeDiff_ = diff; }
    float getX() const { return x_; }
    float getY() const { return y_; }
    int getScaledX() const;
    int getScaledY() const;
    int getKeys() const { return keys_; }
    bool isKeyPressed() const { return keys_ != KEY_NONE; }
    std::string toString() const;

private:
    int timeDiff_;
    int time_;
    float x_, y_;
    int keys_;
};

}  // namespace opsu
