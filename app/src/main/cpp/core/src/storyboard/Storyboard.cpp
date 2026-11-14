#include "opsu/storyboard/Storyboard.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "opsu/util/JavaMath.h"

namespace opsu {

namespace {
int wrapAdd(int a, int b) { return static_cast<int>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int wrapSub(int a, int b) { return static_cast<int>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }

/** Java `(long) d`: saturating, NaN -> 0. */
int64_t toLong(double d) {
    if (d != d) return 0;
    if (d >= 9223372036854775807.0) return std::numeric_limits<int64_t>::max();
    if (d <= -9223372036854775808.0) return std::numeric_limits<int64_t>::min();
    return static_cast<int64_t>(d);
}

using Command = Storyboard::Command;

/** Comparator BY_START: `Double.compare(a.start, b.start)`. */
bool byStart(const Command& a, const Command& b) { return java::compare(a.start, b.start) < 0; }

std::vector<Command> sortedByStart(const std::vector<Command>& in) {
    std::vector<Command> out(in);
    std::stable_sort(out.begin(), out.end(), byStart);  // Collections.sort is stable
    return out;
}

bool visibleAtStartOrEnd(const Command& c) { return c.v0.at(0) > 0 || c.v1.at(0) > 0; }

bool equalsIgnoreCaseAscii(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        unsigned char x = static_cast<unsigned char>(a[i]), y = static_cast<unsigned char>(b[i]);
        if (x >= 'A' && x <= 'Z') x = static_cast<unsigned char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<unsigned char>(y - 'A' + 'a');
        if (x != y) return false;
    }
    return true;
}
}  // namespace

// ------------------------------------------------------------------ Command

Storyboard::Command::Command(int property, AnimationEquation easing, double start, double end, std::vector<float> v0, std::vector<float> v1,
                             const LoopGroup* loop)
    : property(property), easing(easing), start(start), end(end), v0(std::move(v0)), v1(std::move(v1)), loop(loop) {
    if (this->end < this->start) this->end = this->start;
}

// -------------------------------------------------------------- CommandGroup

void Storyboard::CommandGroup::add(int property, AnimationEquation easing, double start, double end, std::vector<float> v0, std::vector<float> v1) {
    addCommand(Command(property, easing, start, end, std::move(v0), std::move(v1), nullptr));
}

void Storyboard::CommandGroup::addCommand(Command c) {
    const double cs = c.start, ce = c.end;
    lists.at(static_cast<size_t>(c.property)).push_back(std::move(c));
    hasCommands = true;
    if (cs < startTime) startTime = cs;
    if (ce > endTime) endTime = ce;
}

Storyboard::LoopGroup::LoopGroup(double startTime, int repeatCount)
    : loopStartTime(startTime), totalIterations(wrapAdd(java::max(0, repeatCount), 1)) {}

void Storyboard::LoopGroup::add(int property, AnimationEquation easing, double start, double end, std::vector<float> v0, std::vector<float> v1) {
    addCommand(Command(property, easing, loopStartTime + start, loopStartTime + end, std::move(v0), std::move(v1), this));
}

// ------------------------------------------------------------------- Sprite

Storyboard::Sprite::Sprite(std::string p, int ox, int oy, float x, float y, bool anim, int frames, double delay, bool loopFwd)
    : path(std::move(p)), originX(ox), originY(oy), initialX(x), initialY(y), isAnimation(anim), frameCount(frames), frameDelay(delay),
      loopForever(loopFwd) {}

Storyboard::LoopGroup& Storyboard::Sprite::addLoopGroup(double start, int repeatCount) {
    loopGroups.push_back(std::make_unique<LoopGroup>(start, repeatCount));
    return *loopGroups.back();
}

Storyboard::TriggerGroup& Storyboard::Sprite::addTriggerGroup(std::string name, double start, double end, int group) {
    triggerGroups.push_back(std::make_unique<TriggerGroup>(std::move(name), start, end, group));
    return *triggerGroups.back();
}

bool Storyboard::Sprite::hasCommands() const {
    if (commands.hasCommands) return true;
    for (const auto& l : loopGroups)
        if (l->hasCommands) return true;
    return false;
}

void Storyboard::Sprite::finish() {
    hasLoopCommands_.fill(false);
    for (int p = 0; p < PROPERTY_COUNT; p++) {
        const size_t ps = static_cast<size_t>(p);
        // each group list is a lazer SortedList (by start time, stable)
        std::vector<Command> all = sortedByStart(commands.lists[ps]);
        std::vector<Command> loopCmds;
        for (const auto& l : loopGroups) {
            std::vector<Command> sorted = sortedByStart(l->lists[ps]);
            loopCmds.insert(loopCmds.end(), sorted.begin(), sorted.end());
        }
        all.insert(all.end(), loopCmds.begin(), loopCmds.end());
        // StoryboardSprite.ApplyTransforms: commands.OrderBy(c => c.StartTime) (stable)
        std::stable_sort(all.begin(), all.end(), byStart);
        for (const Command& c : all)
            if (c.loop != nullptr && c.loop->totalIterations > 1 && c.loop->duration() > 0) hasLoopCommands_[ps] = true;
        merged_[ps] = std::move(all);
    }

    // StartTime
    std::vector<Command> alphaCommands;
    for (const Command& c : sortedByStart(commands.lists[ALPHA])) {
        alphaCommands.push_back(c);
        if (visibleAtStartOrEnd(c)) break;
    }
    for (const auto& l : loopGroups) {
        for (const Command& c : sortedByStart(l->lists[ALPHA])) {
            alphaCommands.push_back(c);
            if (visibleAtStartOrEnd(c)) break;
        }
    }
    double earliest = commands.startTime;
    for (const auto& l : loopGroups) earliest = java::min(earliest, l->startTime);
    earliestTransformTime_ = earliest;
    startTime_ = earliest;
    if (!alphaCommands.empty()) {
        const Command* firstAlpha = nullptr;
        const Command* firstReal = nullptr;
        for (const Command& c : alphaCommands)
            if (firstAlpha == nullptr || c.start < firstAlpha->start) firstAlpha = &c;
        for (const Command& c : alphaCommands)
            if (visibleAtStartOrEnd(c) && (firstReal == nullptr || c.start < firstReal->start)) firstReal = &c;
        if (firstAlpha->v0.at(0) == 0 && firstReal != nullptr) startTime_ = firstReal->start;
    }

    // EndTimeForDisplay
    double latest = commands.endTime;
    for (const auto& l : loopGroups) latest = java::max(latest, l->startTime + l->duration() * l->totalIterations);
    endTimeForDisplay_ = latest;
}

const Storyboard::Command* Storyboard::Sprite::pick(int p, double t, double& effStart) const {
    const std::vector<Command>& list = merged_[static_cast<size_t>(p)];
    if (!hasLoopCommands_[static_cast<size_t>(p)]) {
        int lo = 0, hi = static_cast<int>(list.size()) - 1, idx = -1;
        while (lo <= hi) {
            int mid = (lo + hi) >> 1;
            if (list[static_cast<size_t>(mid)].start <= t) { idx = mid; lo = mid + 1; } else hi = mid - 1;
        }
        if (idx < 0) return nullptr;
        effStart = list[static_cast<size_t>(idx)].start;
        return &list[static_cast<size_t>(idx)];
    }
    const Command* win = nullptr;
    double winStart = -std::numeric_limits<double>::max();
    for (const Command& c : list) {
        if (c.start > t) continue;
        double es = c.start;
        if (c.loop != nullptr && c.loop->totalIterations > 1) {
            double period = c.loop->duration();
            if (period > 0) {
                int64_t k = toLong(std::floor((t - c.start) / period));
                if (k > c.loop->totalIterations - 1) k = c.loop->totalIterations - 1;
                es += static_cast<double>(k) * period;
            }
        }
        if (es >= winStart) {
            win = &c;
            winStart = es;
        }
    }
    if (win != nullptr) effStart = winStart;
    return win;
}

bool Storyboard::Sprite::eval(int p, double t, float* out, int n) {
    const std::vector<Command>& list = merged_[static_cast<size_t>(p)];
    if (list.empty()) return false;
    const Command* win = pick(p, t, tmp_);
    if (win == nullptr) {
        // ApplyInitialValue of the chronologically first command
        for (int i = 0; i < n; i++) out[i] = list[0].v0.at(static_cast<size_t>(i));
        return true;
    }
    double dur = win->duration();
    double prog = (dur <= 0) ? 1 : (t - tmp_) / dur;
    if (prog >= 1) {
        for (int i = 0; i < n; i++) out[i] = win->v1.at(static_cast<size_t>(i));
    } else {
        float e = calc(win->easing, static_cast<float>(prog));
        for (int i = 0; i < n; i++) {
            const float a = win->v0.at(static_cast<size_t>(i)), b = win->v1.at(static_cast<size_t>(i));
            out[i] = a + (b - a) * e;
        }
    }
    return true;
}

bool Storyboard::Sprite::evalBool(int p, double t) {
    const std::vector<Command>& list = merged_[static_cast<size_t>(p)];
    if (list.empty()) return false;
    const Command* win = pick(p, t, tmp_);
    if (win == nullptr) {
        // FlipH / FlipV / Blending only apply their initial value for zero-duration commands
        const Command& first = list[0];
        return first.start == first.end && first.v0.at(0) != 0;
    }
    double dur = win->duration();
    bool ended = (t - tmp_) >= dur;
    return (ended ? win->v1.at(0) : win->v0.at(0)) != 0;
}

void Storyboard::Sprite::evaluate(double t, State& s) {
    float* buf = buf_.data();
    s.x = initialX;
    s.y = initialY;
    if (eval(X, t, buf, 1)) s.x = buf[0];
    if (eval(Y, t, buf, 1)) s.y = buf[0];
    s.scale = 1.f;
    if (eval(SCALE, t, buf, 1)) s.scale = buf[0];
    s.vsx = s.vsy = 1.f;
    if (eval(VECTOR_SCALE, t, buf, 2)) { s.vsx = buf[0]; s.vsy = buf[1]; }
    s.rotation = 0.f;
    if (eval(ROTATION, t, buf, 1)) s.rotation = buf[0];
    s.r = s.g = s.b = 1.f;
    if (eval(COLOUR, t, buf, 3)) { s.r = buf[0]; s.g = buf[1]; s.b = buf[2]; }
    s.alpha = 1.f;
    if (eval(ALPHA, t, buf, 1)) s.alpha = buf[0];
    // In stable, alpha above 1 made sprites disappear; storyboarders use it to flicker (lazer reproduces it).
    if (s.alpha > 1.f) s.alpha = std::fmod(s.alpha, 1.f);
    s.additive = evalBool(BLEND, t);
    s.flipH = evalBool(FLIP_H, t);
    s.flipV = evalBool(FLIP_V, t);
}

// --------------------------------------------------------------- Storyboard

Storyboard::Storyboard() {
    auto add = [this](std::unique_ptr<Layer> l) { layerIndex_[l->name] = l.get(); layers_.push_back(std::move(l)); };
    add(std::make_unique<Layer>("Video", 4, false));
    add(std::make_unique<Layer>("Background", 3, true));
    auto fail = std::make_unique<Layer>("Fail", 2, true);
    fail->visibleWhenPassing = false;
    add(std::move(fail));
    auto pass = std::make_unique<Layer>("Pass", 1, true);
    pass->visibleWhenFailing = false;
    add(std::move(pass));
    add(std::make_unique<Layer>("Foreground", minimumLayerDepth_ = 0, true));
    add(std::make_unique<Layer>("Overlay", std::numeric_limits<int>::min(), true));
}

Storyboard::Layer& Storyboard::getLayer(const std::string& name) {
    auto it = layerIndex_.find(name);
    if (it != layerIndex_.end()) return *it->second;
    minimumLayerDepth_ = wrapSub(minimumLayerDepth_, 1);
    auto l = std::make_unique<Layer>(name, minimumLayerDepth_, true);
    Layer* raw = l.get();
    layerIndex_[name] = raw;
    layers_.push_back(std::move(l));
    return *raw;
}

bool Storyboard::hasDrawable() const {
    for (const auto& l : layers_)
        for (const auto& s : l->sprites)
            if (s->isDrawable()) return true;
    return false;
}

bool Storyboard::hasSamples() const {
    for (const auto& l : layers_)
        if (!l->samples.empty()) return true;
    return false;
}

bool Storyboard::replacesBackground(const std::optional<std::string>& backgroundFile) {
    if (!backgroundFile || backgroundFile->empty()) return false;
    for (const auto& s : getLayer("Background").sprites)
        if (equalsIgnoreCaseAscii(s->path, *backgroundFile)) return true;
    return false;
}

const std::vector<Storyboard::Layer*>& Storyboard::getDrawOrder() {
    if (!drawOrder_) {
        std::vector<Layer*> order;
        for (const auto& l : layers_) order.push_back(l.get());
        std::stable_sort(order.begin(), order.end(), [](const Layer* a, const Layer* b) { return b->depth < a->depth; });  // Integer.compare(b, a)
        drawOrder_ = std::move(order);
    }
    return *drawOrder_;
}

void Storyboard::finish() {
    for (const auto& l : layers_)
        for (const auto& s : l->sprites) s->finish();
    drawOrder_.reset();
}

}  // namespace opsu
