#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "opsu/ui/animations/AnimationEquation.h"

namespace opsu {

/**
 * Storyboard data model, a port of osu!lazer's `osu.Game.Storyboards` namespace (Storyboard, StoryboardLayer, StoryboardSprite,
 * StoryboardAnimation, StoryboardCommandGroup, StoryboardLoopingGroup, StoryboardTriggerGroup, StoryboardSampleInfo and the command classes).
 * Port of itdelatrisu.opsu.storyboard.Storyboard. Pure data + evaluation: no rendering (StoryboardRenderer is not ported) and no parsing
 * (StoryboardParser is the next batch; it fills this model through the members below).
 *
 * Seams / deviations
 *  - Java "package-private" members (the Command constructor, `CommandGroup::lists`, `Sprite` constructor, `addLoopGroup`, `addTriggerGroup`, `Layer`
 *    constructor, `Storyboard::finish`) are PUBLIC here, because the parser is a different class. Do not call them from anywhere else.
 *  - Ownership: Java objects are shared references. `Layer` / `Sprite` / `LoopGroup` / `TriggerGroup` are owned by `unique_ptr`s (stable addresses), so
 *    `Command::loop` is a non-owning pointer into the owning sprite's `loopGroups`, and the classes are not copyable. `Sprite::finish()` builds the
 *    merged, time-ordered command lists as COPIES of the commands (Java kept references); call `finish()` after the last command was added (the
 *    Java never re-finishes either).
 *  - `float[] v0, v1` are `std::vector<float>` and are read with `.at()`: a command with too few values throws `std::out_of_range` where Java throws
 *    ArrayIndexOutOfBoundsException (a well-formed parser never makes one).
 *  - `Collections.sort` (stable, `Double.compare` / `Integer.compare`) is `std::stable_sort` with the same comparison (-0.0 < 0.0, NaN last).
 *  - `LinkedHashMap<String, Layer>` is an insertion-ordered vector plus a name index. `replacesBackground` compares paths ASCII case-insensitively
 *    (Java `equalsIgnoreCase` also folds non-ASCII letters; storyboard paths with such letters differing only by case would be missed).
 *  - A nullable `String` argument is `std::optional<std::string>`. Java int arithmetic that can overflow (`repeatCount + 1`, `--minimumLayerDepth`) wraps.
 *  - `Sprite::evaluate` / `Sprite::pick` use per-sprite scratch buffers (not thread safe, same as Java).
 */
class Storyboard {
public:
    /** Command property indices, in the order of lazer's `StoryboardCommandGroup.AllCommands`. */
    static constexpr int X = 0, Y = 1, SCALE = 2, VECTOR_SCALE = 3, ROTATION = 4, COLOUR = 5, ALPHA = 6, BLEND = 7, FLIP_H = 8, FLIP_V = 9,
                         PROPERTY_COUNT = 10;

    class LoopGroup;

    /** A single storyboard command (lazer: `StoryboardCommand<T>`). */
    struct Command {
        int property = 0;
        AnimationEquation easing = AnimationEquation::LINEAR;
        double start = 0, end = 0;
        /** Start / end values: 1 float (most), 2 (vector scale), 3 (colour), or 1 as 0/1 (flip, blend). */
        std::vector<float> v0, v1;
        /** Set when this command belongs to a looping group (lazer: `StoryboardLoopingCommand`); non-owning. */
        const LoopGroup* loop = nullptr;

        Command() = default;
        /** `end < start` is clamped to `start`. */
        Command(int property, AnimationEquation easing, double start, double end, std::vector<float> v0, std::vector<float> v1, const LoopGroup* loop);

        double duration() const { return end - start; }
    };

    /** A group of commands (lazer: `StoryboardCommandGroup`). */
    class CommandGroup {
    public:
        std::array<std::vector<Command>, PROPERTY_COUNT> lists;  ///< per property, in insertion order
        double startTime = std::numeric_limits<double>::max();
        double endTime = -std::numeric_limits<double>::max();
        bool hasCommands = false;

        CommandGroup() = default;
        virtual ~CommandGroup() = default;

        /** Adds a command (overridden by looping groups to apply the loop offset). */
        virtual void add(int property, AnimationEquation easing, double start, double end, std::vector<float> v0, std::vector<float> v1);
        void addCommand(Command c);
        double duration() const { return endTime - startTime; }
    };

    /** A looping group (lazer: `StoryboardLoopingGroup`). */
    class LoopGroup : public CommandGroup {
    public:
        const double loopStartTime;
        /** Total number of times this loop is played back (always > 0). */
        const int totalIterations;

        LoopGroup(double startTime, int repeatCount);
        void add(int property, AnimationEquation easing, double start, double end, std::vector<float> v0, std::vector<float> v1) override;
    };

    /** Trigger groups are parsed but, exactly like lazer, never applied. */
    class TriggerGroup : public CommandGroup {
    public:
        const std::string triggerName;
        const double triggerStart, triggerEnd;
        const int groupNumber;

        TriggerGroup(std::string name, double start, double end, int group)
            : triggerName(std::move(name)), triggerStart(start), triggerEnd(end), groupNumber(group) {}
    };

    /** Evaluated sprite state at a point in time. */
    struct State {
        float x = 0, y = 0;
        /** Uniform scale (`S` command), multiplied with vsx / vsy. */
        float scale = 0;
        float vsx = 0, vsy = 0;
        /** Rotation in degrees. */
        float rotation = 0;
        float r = 0, g = 0, b = 0;
        float alpha = 0;
        bool additive = false, flipH = false, flipV = false;
    };

    /** A storyboard sprite or animation (lazer: `StoryboardSprite` / `StoryboardAnimation`). */
    class Sprite {
    public:
        const std::string path;
        /** Origin as (0,1,2) per axis: left / centre / right and top / centre / bottom. */
        const int originX, originY;
        const float initialX, initialY;

        const bool isAnimation;
        const int frameCount;
        const double frameDelay;
        const bool loopForever;

        CommandGroup commands;
        std::vector<std::unique_ptr<LoopGroup>> loopGroups;
        std::vector<std::unique_ptr<TriggerGroup>> triggerGroups;

        Sprite(std::string path, int originX, int originY, float x, float y, bool isAnimation, int frameCount, double frameDelay, bool loopForever);
        Sprite(const Sprite&) = delete;
        Sprite& operator=(const Sprite&) = delete;

        LoopGroup& addLoopGroup(double start, int repeatCount);
        TriggerGroup& addTriggerGroup(std::string name, double start, double end, int group);

        bool hasCommands() const;
        /** Whether this element can be drawn (lazer: `IsDrawable`). */
        bool isDrawable() const { return hasCommands(); }
        /** Lifetime start (lazer: `StoryboardSprite.StartTime`); 0 before finish(). */
        double getStartTime() const { return startTime_; }
        /** Lifetime end (lazer: `EndTimeForDisplay`); 0 before finish(). */
        double getEndTimeForDisplay() const { return endTimeForDisplay_; }
        /** Lazer: `EarliestTransformTime`; animations are synchronised to this. */
        double getEarliestTransformTime() const { return earliestTransformTime_; }

        /** Precomputes the chronologically ordered command lists and lifetimes. */
        void finish();

        /** Returns whether this sprite is alive (should be drawn / updated) at the given time. */
        bool isAlive(double t) const { return t >= startTime_ && t < endTimeForDisplay_; }

        /** Evaluates the full sprite state (not thread safe). Requires finish(). */
        void evaluate(double t, State& s);

    private:
        std::array<std::vector<Command>, PROPERTY_COUNT> merged_;
        std::array<bool, PROPERTY_COUNT> hasLoopCommands_{};
        double startTime_ = 0, endTimeForDisplay_ = 0, earliestTransformTime_ = 0;
        std::array<float, 3> buf_{};
        double tmp_ = 0;

        const Command* pick(int p, double t, double& effStart) const;
        bool eval(int p, double t, float* out, int n);
        bool evalBool(int p, double t);
    };

    /** A storyboard sample (lazer: `StoryboardSampleInfo`). */
    struct Sample {
        std::string path;
        double time;
        int volume;
        Sample(std::string path, double time, int volume) : path(std::move(path)), time(time), volume(volume) {}
    };

    /** A storyboard layer (lazer: `StoryboardLayer`). */
    class Layer {
    public:
        const std::string name;
        const int depth;
        const bool masking;
        bool visibleWhenPassing = true;
        bool visibleWhenFailing = true;
        std::vector<std::unique_ptr<Sprite>> sprites;
        std::vector<Sample> samples;

        Layer(std::string name, int depth, bool masking) : name(std::move(name)), depth(depth), masking(masking) {}
    };

    /** Whether the storyboard uses the widescreen (16:9) area instead of 4:3. */
    bool widescreen = false;
    /** Whether sprites should prefer skin textures. */
    bool useSkinSprites = false;

    Storyboard();
    Storyboard(const Storyboard&) = delete;
    Storyboard& operator=(const Storyboard&) = delete;

    /** The layer of that name; created (depth = one below the lowest custom layer so far) if it does not exist. */
    Layer& getLayer(const std::string& name);

    /** Lazer: `HasDrawable`. */
    bool hasDrawable() const;
    /** Whether any sample exists in any layer. */
    bool hasSamples() const;
    /** Lazer: `ReplacesBackground` (case-insensitive path match in the Background layer; null / empty = false). */
    bool replacesBackground(const std::optional<std::string>& backgroundFile);

    /** Layers from back to front (lazer: higher depth is further back). Cached until finish(). */
    const std::vector<Layer*>& getDrawOrder();

    /** Finalises all sprites after parsing. */
    void finish();

private:
    std::vector<std::unique_ptr<Layer>> layers_;       // insertion order (LinkedHashMap)
    std::map<std::string, Layer*> layerIndex_;
    int minimumLayerDepth_ = 0;
    std::optional<std::vector<Layer*>> drawOrder_;
};

}  // namespace opsu
