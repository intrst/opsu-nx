#pragma once
// Port of itdelatrisu.opsu.options.OptionGroup.
// Not ported: getIcon() (returns a fake.Image from GameImage). The icon is exposed as an IconKind instead; the UI maps it
// to GameImage.MENU_NAV_<kind> when GameImage exists.
#include <string>
#include <vector>

#include "opsu/options/Options.h"

namespace opsu {

class OptionGroup {
public:
    /** Java: GameImage.MENU_NAV_GENERAL ... MENU_NAV_CUSTOM. */
    enum class IconKind { NONE, GENERAL, GRAPHICS, GAMEPLAY, AUDIO, SKIN, INPUT, CUSTOM };

    /** A group with options. */
    OptionGroup(std::string category, std::vector<GameOption> options)
        : category_(std::move(category)), options_(std::move(options)) {}
    /** A section header (icon, no options). */
    OptionGroup(std::string category, IconKind icon) : category_(std::move(category)), icon_(icon) {}

    const std::string& getName() const { return category_; }
    const std::vector<GameOption>& getOptions() const { return options_; }
    const GameOption& getOption(int i) const { return options_.at(static_cast<size_t>(i)); }
    /** True for a section header. */
    bool hasIcon() const { return icon_ != IconKind::NONE; }
    IconKind getIconKind() const { return icon_; }
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

    /**
     * OptionGroup.ALL_OPTIONS, in display order: a section header followed by its sub-groups.
     * Built on first use (not at static-init time) from the isDesktop seam: SCREEN_RESOLUTION and TARGET_FPS are
     * desktop-only. Install the seams before the first call. UI-thread only (the visible flags are mutable).
     */
    static std::vector<OptionGroup>& allOptions();
    /** OptionGroup.LOADER_OPTIONS (quick settings on the beatmap loading screen). */
    static std::vector<OptionGroup>& loaderOptions();
    /** Drops the cached lists so the next allOptions()/loaderOptions() call rebuilds them (tests, seam changes). */
    static void rebuild();

private:
    std::string category_;
    IconKind icon_ = IconKind::NONE;
    std::vector<GameOption> options_;
    bool visible_ = true;
};

}  // namespace opsu
