// Port of itdelatrisu/opsu/GameImage.java (T11). Header: opsu/GameImage.h. The entry table is generated (GameImageTable.cpp).
// Not thread safe (render thread only).
#include "opsu/GameImage.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <system_error>

#include "opsu/logging/Log.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaMath.h"

namespace fs = std::filesystem;

namespace opsu {
namespace gameimage {
namespace {

using gameimage_detail::GameImageInfo;

constexpr int UNSCALED_HEIGHT = 768;
constexpr float GLOW_REFERENCE_HUE = 213.f;

/** The mutable part of one Java enum constant. Empty vector == Java null array. */
struct State {
    std::shared_ptr<Image> defaultImage, skinImage;
    std::vector<std::shared_ptr<Image>> defaultImages, skinImages;
    bool isSkinned = false, absentInSkin = false;
    int rawWidth = 0, rawHeight = 0;
    std::shared_ptr<Image> builtinPlain;       // getPlainImage() cache
    const SkinInfo* builtinPlainSkin = nullptr;
    float builtinPlainScale = 0.f;
};

struct Registry {
    GameImageHost host;
    std::shared_ptr<const SkinInfo> skin = std::make_shared<SkinInfo>();
    int containerWidth = 0, containerHeight = 0;
    float uiscale = 0.f;
    std::vector<State> states;
    Registry() : states(static_cast<size_t>(gameimage_detail::kGameImageCount)) {}
};

Registry& R() {
    static Registry r;
    return r;
}

State& S(GameImage g) { return R().states[static_cast<size_t>(g)]; }
const GameImageInfo& I(GameImage g) { return gameimage_detail::kGameImageTable[static_cast<int>(g)]; }

// ---- host seam defaults ---------------------------------------------------------------------------------------------
float mobileUIScale(float def) {
    const auto& f = R().host.getMobileUIScale;
    return f ? f(def) : def;
}
bool loadHDImages() {
    const auto& f = R().host.loadHDImages;
    return f ? f() : false;
}
bool beatmapSkinIgnored() {
    const auto& f = R().host.isBeatmapSkinIgnored;
    return f ? f() : false;
}
bool resourceExists(const std::string& name) {
    const auto& f = R().host.resourceExists;
    return f ? f(name) : false;
}

// ---- small helpers --------------------------------------------------------------------------------------------------
char lowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
std::string lower(std::string s) {
    for (char& c : s) c = lowerAscii(c);
    return s;
}
bool equalsIgnoreCase(const std::string& a, const std::string& b) { return lower(a) == lower(b); }
bool endsWith(const std::string& s, const std::string& end) {
    return s.size() >= end.size() && s.compare(s.size() - end.size(), end.size(), end) == 0;
}
bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

/** Java String.split("/"): trailing empty strings are dropped. */
std::vector<std::string> splitSlash(const std::string& s) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        size_t p = s.find('/', start);
        if (p == std::string::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    while (!parts.empty() && parts.back().empty()) parts.pop_back();
    return parts;
}

std::string stripTrailingSlashes(std::string s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

bool isFile(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}
bool isDirectory(const fs::path& p) {
    std::error_code ec;
    return fs::is_directory(p, ec);
}
/** File.list(): the names in a directory; false if it is not a readable directory. */
bool listDir(const fs::path& dir, std::vector<std::string>& out) {
    std::error_code ec;
    fs::directory_iterator it(dir, ec);
    if (ec) return false;
    for (; it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) break;
        out.push_back(it->path().filename().string());
    }
    return true;
}
/** File.getAbsolutePath(). */
std::string absolutePath(const fs::path& p) {
    std::error_code ec;
    fs::path a = fs::absolute(p, ec);
    return (ec ? p : a).string();
}

/** String.format(filenameFormat + suffix, i) for the only conversion the table uses, "%d". */
std::string formatFrame(const std::string& fmt, int i) {
    std::string out = fmt;
    size_t p = out.find("%d");
    if (p != std::string::npos) out.replace(p, 2, std::to_string(i));
    return out;
}

// ---- font sprites (default-N, score-N, combo-N) ---------------------------------------------------------------------
enum FontKind { FONT_NONE = 0, FONT_HITCIRCLE = 1, FONT_SCORE = 2, FONT_COMBO = 3 };
const char* const FONT_FOLDERS[] = {nullptr, "hitcircle", "score", "combo"};
const char* const FONT_DEFAULT_PREFIX[] = {nullptr, "default", "score", "score"};

int getFontKind(const std::string& filename) {
    if (startsWith(filename, "default-")) return FONT_HITCIRCLE;
    if (startsWith(filename, "score-")) return FONT_SCORE;
    if (startsWith(filename, "combo-")) return FONT_COMBO;
    return FONT_NONE;
}

/** skin.ini prefix of a font kind for `dir`, or "" (Java null) if `dir` is not the active game skin folder. */
bool getSkinFontPrefix(int kind, const std::string& dir, std::string& out) {
    const SkinInfo* skin = R().skin.get();
    if (!skin) return false;
    std::string sd = skin->getDirectory();
    if (sd.empty() || stripTrailingSlashes(sd) != stripTrailingSlashes(dir)) return false;  // beatmap skin folders: only the folders are detected
    switch (kind) {
        case FONT_HITCIRCLE: out = skin->getHitCircleFontPrefix(); return true;
        case FONT_SCORE: out = skin->getScoreFontPrefix(); return true;
        case FONT_COMBO: out = skin->getComboFontPrefix(); return true;
        default: return false;
    }
}

/** Finds a direct child of a directory, ignoring case. Returns "" if there is none. */
std::string findChildIgnoreCase(const std::string& parent, const std::string& name, bool dirWanted) {
    if (parent.empty() || name.empty()) return "";
    fs::path exact = fs::path(parent) / name;
    if (dirWanted ? isDirectory(exact) : isFile(exact)) return exact.string();
    std::vector<std::string> list;
    if (!listDir(parent, list)) return "";
    for (const std::string& n : list) {
        if (equalsIgnoreCase(n, name)) {
            fs::path f = fs::path(parent) / n;
            if (dirWanted ? isDirectory(f) : isFile(f)) return f.string();
        }
    }
    return "";
}

/** "[stem].png/.jpg" (case-insensitive) inside a directory; "" if none. */
std::string findImageIgnoreCase(const std::string& folder, const std::string& stem, unsigned char type) {
    if (type & IMG_PNG) {
        std::string f = findChildIgnoreCase(folder, stem + ".png", false);
        if (!f.empty()) return absolutePath(f);
    }
    if (type & IMG_JPG) {
        std::string f = findChildIgnoreCase(folder, stem + ".jpg", false);
        if (!f.empty()) return absolutePath(f);
    }
    return "";
}

/** "score-comma@2x" -> "comma@2x". */
std::string getFontGlyph(const std::string& filename) { return filename.substr(filename.find('-') + 1); }

std::string findFontByPrefix(const std::string& filename, int kind, const std::string& dir, unsigned char type) {
    std::string prefix;
    if (!getSkinFontPrefix(kind, dir, prefix)) return "";
    prefix = java::trim(prefix);
    std::replace(prefix.begin(), prefix.end(), '\\', '/');
    if (prefix.empty() || equalsIgnoreCase(prefix, FONT_DEFAULT_PREFIX[kind])) return "";

    // walk the folders of the prefix; the last segment is the start of the file name
    std::vector<std::string> parts = splitSlash(prefix);
    std::string folder = dir, last;
    bool haveLast = false;
    for (size_t i = 0; i < parts.size(); i++) {
        std::string part = java::trim(parts[i]);
        if (part.empty() || part == ".") continue;
        if (part == "..") return "";
        if (i == parts.size() - 1) {
            last = part;
            haveLast = true;
        } else {
            folder = findChildIgnoreCase(folder, part, true);
            if (folder.empty()) return "";
        }
    }
    if (!haveLast) return "";
    return findImageIgnoreCase(folder, last + "-" + getFontGlyph(filename), type);
}

std::string findFontInFolder(const std::string& filename, int kind, const std::string& dir, unsigned char type) {
    if (dir.empty()) return "";
    std::string folderName = FONT_FOLDERS[kind];
    std::string folder;
    std::string fonts = findChildIgnoreCase(dir, "fonts", true);
    if (!fonts.empty()) folder = findChildIgnoreCase(fonts, folderName, true);
    if (folder.empty()) return "";

    std::string glyph = getFontGlyph(filename);
    // 1. the skin.ini prefix name, 2. the built-in name(s)
    std::vector<std::string> stems;
    std::string prefix;
    if (getSkinFontPrefix(kind, dir, prefix)) {
        std::string p = java::trim(prefix);
        std::replace(p.begin(), p.end(), '\\', '/');
        p = p.substr(p.rfind('/') == std::string::npos ? 0 : p.rfind('/') + 1);
        if (!p.empty()) stems.push_back(p);
    }
    stems.push_back(folderName);                  // hitcircle-0, score-0, combo-0
    stems.push_back(FONT_DEFAULT_PREFIX[kind]);   // default-0, score-0
    for (const std::string& stem : stems) {
        std::string found = findImageIgnoreCase(folder, stem + "-" + glyph, type);
        if (!found.empty()) return found;
    }

    // 3. any "<name>-<glyph>" file in the folder (sorted so the result is stable)
    std::vector<std::string> list;
    if (!listDir(folder, list)) return "";
    std::stable_sort(list.begin(), list.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
    std::string end = lower("-" + glyph);
    for (const std::string& n : list) {
        std::string l = lower(n);
        bool ext = ((type & IMG_PNG) && endsWith(l, ".png")) || ((type & IMG_JPG) && endsWith(l, ".jpg"));
        if (!ext) continue;
        std::string noExt = l.substr(0, l.size() - 4);
        if (endsWith(noExt, end)) {
            fs::path f = fs::path(folder) / n;
            if (isFile(f)) return absolutePath(f);
        }
    }
    return "";
}

/**
 * Returns the image file name, with extension, by first looking through the given directory ("" = none) and then the
 * default resource locations (unless dirOnly and a directory is given). Built-in images come back as the asset path
 * "res/<name>". Returns "" (Java null) if nothing was found.
 */
std::string getImageFileName(const std::string& filename, const std::string& dir, unsigned char type, bool dirOnly) {
    std::vector<std::string> names;
    if (type & IMG_PNG) names.push_back(filename + ".png");
    if (type & IMG_JPG) names.push_back(filename + ".jpg");

    // font sprites: honour the skin.ini prefix first (e.g. "fonts/score/score")
    int fontKind = !dir.empty() ? getFontKind(filename) : FONT_NONE;
    if (fontKind != FONT_NONE) {
        std::string found = findFontByPrefix(filename, fontKind, dir, type);
        if (!found.empty()) return found;
    }

    // look through directory (osu! never looks for combo-N in the skin root: ComboPrefix defaults to "score")
    if (!dir.empty() && fontKind != FONT_COMBO) {
        for (const std::string& n : names) {
            fs::path file = fs::path(dir) / n;
            int listed = R().host.isListed ? R().host.isListed(dir, n) : -1;  // skip the file system for known misses
            if ((listed == -1 || listed == 1) && isFile(file)) return absolutePath(file);
        }
    }

    // not in the skin root: the skin's "fonts" sub-folders
    if (fontKind != FONT_NONE) {
        std::string found = findFontInFolder(filename, fontKind, dir, type);
        if (!found.empty()) return found;
    }

    // default resource path
    if (!dirOnly || dir.empty()) {
        for (const std::string& n : names) {
            if (resourceExists(n)) return "res/" + n;
        }
    }
    return "";
}

std::vector<std::string> getSuffixes() {
    if (loadHDImages() && R().uiscale >= 1) return {HD_SUFFIX, ""};
    return {""};
}

bool keepsPlainImage(GameImage g) {
    return g == GameImage::HIT_MISS || g == GameImage::HIT_50 || g == GameImage::HIT_100 || g == GameImage::HIT_300 ||
           g == GameImage::HIT_100K || g == GameImage::HIT_300K || g == GameImage::HIT_300G;
}

// ---- loading ----------------------------------------------------------------------------------------------------------
/** Attempts to load multiple Images; empty = not found. dir "" = the default resource locations. */
std::vector<std::shared_ptr<Image>> loadImageArray(GameImage g, const std::string& dir) {
    const GameImageInfo& info = I(g);
    std::string format = info.filenameFormat;
    if (!format.empty()) {
        for (const std::string& suffix : getSuffixes()) {
            std::vector<std::shared_ptr<Image>> list;
            int i = 0;
            for (;;) {
                // look for next image
                std::string formatted = formatFrame(format + suffix, i++);
                std::string name = getImageFileName(formatted, dir, info.type, true);
                if (name.empty()) break;
                try {
                    auto img = std::make_shared<Image>(name);
                    if (suffix == HD_SUFFIX) img = std::make_shared<Image>(img->getScaledCopy(0.5f));
                    list.push_back(std::move(img));
                } catch (const std::exception& e) {
                    Log::error("GameImage", "Failed to set image '" + name + "'.", e);
                    break;
                }
            }
            if (!list.empty()) return list;
        }
    }
    return {};
}

/** Hue (degrees) the built-in sprite must be rotated to match the skin's MenuGlow, or 0. */
float getGlowHueShift(GameImage g) {
    if (g != GameImage::MENU_LOGO && g != GameImage::MENU_PLAY && g != GameImage::MENU_EXIT && g != GameImage::DOWNLOADS) return 0.f;
    const SkinInfo* skin = R().skin.get();
    if (!skin || !skin->hasCustomMenuGlow()) return 0.f;
    Color glow = skin->getMenuGlowColor();
    float max = java::max(glow.r, java::max(glow.g, glow.b));
    float min = java::min(glow.r, java::min(glow.g, glow.b));
    float d = max - min;
    if (d < 0.05f) return 0.f;  // white/grey glow has no hue to match
    float hue;
    if (max == glow.r)
        hue = std::fmod((glow.g - glow.b) / d, 6.f);
    else if (max == glow.g)
        hue = (glow.b - glow.r) / d + 2.f;
    else
        hue = (glow.r - glow.g) / d + 4.f;
    hue = std::fmod(std::fmod(hue * 60.f, 360.f) + 360.f, 360.f);
    float shift = hue - GLOW_REFERENCE_HUE;
    if (shift > 180.f)
        shift -= 360.f;
    else if (shift <= -180.f)
        shift += 360.f;
    return (std::fabs(shift) < 1.f) ? 0.f : shift;
}

/** Attempts to load a single Image; null = not found. dir "" = the default resource locations. */
std::shared_ptr<Image> loadImageSingle(GameImage g, const std::string& dir) {
    const GameImageInfo& info = I(g);
    for (const std::string& suffix : getSuffixes()) {
        std::string name = getImageFileName(std::string(info.filename) + suffix, dir, info.type, true);
        if (!name.empty()) {
            try {
                // built-in sprite: rotate its hue to the skin's MenuGlow, if it has one
                float hue = dir.empty() ? getGlowHueShift(g) : 0.f;
                auto img = (hue != 0.f) ? std::make_shared<Image>(name, hue) : std::make_shared<Image>(name);
                if (suffix == HD_SUFFIX) img = std::make_shared<Image>(img->getScaledCopy(0.5f));
                return img;
            } catch (const std::exception& e) {
                Log::error("GameImage", std::string("Failed to set image '") + info.filename + "'.", e);
            }
        }
    }
    return nullptr;
}

// ---- processing ---------------------------------------------------------------------------------------------------------
std::shared_ptr<Image> scaledCopy(const Image& img, float f) { return std::make_shared<Image>(img.getScaledCopy(f)); }
std::shared_ptr<Image> scaledCopy(const Image& img, float w, float h) { return std::make_shared<Image>(img.getScaledCopy(w, h)); }

int fontLineHeightSum() {
    const GameImageHost& h = R().host;
    int large = h.fontLargeLineHeight ? h.fontLargeLineHeight() : 0;
    int def = h.fontDefaultLineHeight ? h.fontDefaultLineHeight() : 0;
    return large + def;
}

/** Java process_sub(img, w, h) of the entry: the override, or the identity. */
std::shared_ptr<Image> processSub(GameImage g, const std::shared_ptr<Image>& imgp, int w, int h) {
    const GameImageInfo& info = I(g);
    Image& img = *imgp;
    switch (info.proc) {
        case ProcKind::None:
            return imgp;
        case ProcKind::ScaleMobileUI:
            return scaledCopy(img, mobileUIScale(info.p1));
        case ProcKind::ScaleMin1pxMobileUI:
            return std::make_shared<Image>(scaleMin1px(img, mobileUIScale(info.p1)));
        case ProcKind::ScaleFixed:
            return scaledCopy(img, info.p1);
        case ProcKind::FitHeightDiv:
            return scaledCopy(img, (static_cast<float>(h) / info.p1) / static_cast<float>(img.getHeight()));
        case ProcKind::FitHeightMul:
            return scaledCopy(img, (static_cast<float>(h) * info.p1) / static_cast<float>(img.getHeight()));
        case ProcKind::FitHeightMulMobileUI:
            return scaledCopy(img, (static_cast<float>(h) * info.p1) / static_cast<float>(img.getHeight()) * mobileUIScale(info.p2));
        case ProcKind::FitWidth:
            return scaledCopy(img, static_cast<float>(w) / static_cast<float>(img.getWidth()));
        case ProcKind::StretchWH:
            return scaledCopy(img, static_cast<float>(w), static_cast<float>(h));
        case ProcKind::StretchWHAlpha:
            img.setAlpha(info.p1);
            return scaledCopy(img, static_cast<float>(w), static_cast<float>(h));
        case ProcKind::FitButtonBgHeight: {
            auto bg = getImage(GameImage::MENU_BUTTON_BG);
            if (!bg) {  // Java: NullPointerException; keep the image unscaled instead of crashing
                Log::error("GameImage", std::string("MENU_BUTTON_BG is missing, '") + info.name + "' is left unscaled.");
                return imgp;
            }
            return scaledCopy(img, (static_cast<float>(bg->getHeight()) * info.p1) / static_cast<float>(img.getHeight()));
        }
        case ProcKind::MenuTab: {
            float th = static_cast<float>(h) * 0.033f * mobileUIScale(0.5f);
            float tw = th * 222.f / 42.f;
            return scaledCopy(img, static_cast<float>(java::max(1, java::round(tw))), static_cast<float>(java::max(1, java::round(th))));
        }
        case ProcKind::MusicNoteFontSquare: {
            int r = java::toInt(static_cast<double>(static_cast<float>(fontLineHeightSum() - 8) / getUIscale()));
            return scaledCopy(img, static_cast<float>(r), static_cast<float>(r));
        }
        case ProcKind::LoaderFontScale: {
            int r = java::toInt(static_cast<double>(static_cast<float>(fontLineHeightSum() - 8) / getUIscale()));
            return scaledCopy(img, static_cast<float>(r) / 48.f);
        }
        case ProcKind::MenuBgCover: {
            img.setAlpha(0.9f);
            // "cover" fit: keep the aspect ratio and fill the whole screen
            float scale = std::max(static_cast<float>(w) / static_cast<float>(img.getWidth()), static_cast<float>(h) / static_cast<float>(img.getHeight()));
            int sw = java::max(w, java::toInt(std::ceil(static_cast<double>(static_cast<float>(img.getWidth()) * scale))));
            int sh = java::max(h, java::toInt(std::ceil(static_cast<double>(static_cast<float>(img.getHeight()) * scale))));
            return scaledCopy(img, static_cast<float>(sw), static_cast<float>(sh));
        }
    }
    return imgp;
}

/** Applies the UI scale to a processed image, unless the image keeps its loaded size. */
std::shared_ptr<Image> finishImage(const std::shared_ptr<Image>& img, bool scaled) {
    return scaled ? std::make_shared<Image>(scaleMin1px(*img, getUIscale())) : img;
}

/** Performs the post-loading actions on the image(s) of an entry (Java process()). */
void process(GameImage g) {
    Registry& r = R();
    if (r.containerHeight == 0) throw std::logic_error("GameImage::init() was not called (Java: ArithmeticException)");
    State& s = S(g);
    int unscaledWidth = UNSCALED_HEIGHT * r.containerWidth / r.containerHeight;
    std::shared_ptr<Image> loaded = s.skinImage ? s.skinImage : s.defaultImage;
    if (loaded) {
        s.rawWidth = loaded->getWidth();
        s.rawHeight = loaded->getHeight();
    }
    bool scaled = I(g).scalesWithUI;  // false: keep the loaded pixel size
    if (!s.skinImages.empty() || (!s.defaultImages.empty() && !s.skinImage)) {
        size_t n = !s.skinImages.empty() ? s.skinImages.size() : s.defaultImages.size();
        for (size_t i = 0; i < n; i++) {
            const auto& src = getImages(g);  // Java: getImages()[i] (setImage below writes to the beatmap array if it exists)
            if (i >= src.size() || !src[i]) continue;
            std::shared_ptr<Image> out = finishImage(processSub(g, src[i], unscaledWidth, UNSCALED_HEIGHT), scaled);
            setImage(g, out, static_cast<int>(i));
        }
    } else {
        std::shared_ptr<Image> cur = getImage(g);
        if (cur) setImage(g, finishImage(processSub(g, cur, unscaledWidth, UNSCALED_HEIGHT), scaled));
    }

    // the plain image next to an animation (hit300 next to hit300-0)
    if (!s.skinImages.empty() || (!s.defaultImages.empty() && !s.skinImage)) {
        std::shared_ptr<Image> plain = !s.skinImages.empty() ? s.skinImage : s.defaultImage;
        if (plain) setImage(g, finishImage(processSub(g, plain, unscaledWidth, UNSCALED_HEIGHT), scaled));
    }
}

/** Loads the default image(s) from a directory ("" = default resource locations); true if something was loaded. */
bool loadDefaultFrom(GameImage g, const std::string& dir) {
    State& s = S(g);
    if (*I(g).filenameFormat) {
        s.defaultImages = loadImageArray(g, dir);
        if (!s.defaultImages.empty()) {
            if (keepsPlainImage(g)) s.defaultImage = loadImageSingle(g, dir);  // may be null
            return true;
        }
    }
    s.defaultImage = loadImageSingle(g, dir);
    return s.defaultImage != nullptr;
}

}  // namespace

// ---- seams ----------------------------------------------------------------------------------------------------------------
void setHost(const GameImageHost& host) { R().host = host; }
void setSkinInfo(std::shared_ptr<const SkinInfo> skin) { R().skin = std::move(skin); }
const SkinInfo* skinInfo() { return R().skin.get(); }

// ---- static ---------------------------------------------------------------------------------------------------------------
void init(int width, int height) {
    Registry& r = R();
    r.containerWidth = width;
    r.containerHeight = height;
    r.uiscale = static_cast<float>(r.containerHeight) / static_cast<float>(UNSCALED_HEIGHT);
}

float getUIscale() { return R().uiscale; }

void clearReferences() {
    for (State& s : R().states) {
        s.defaultImage = nullptr;
        s.skinImage = nullptr;
        s.defaultImages.clear();
        s.skinImages.clear();
        s.isSkinned = false;
        s.absentInSkin = false;
        s.rawWidth = s.rawHeight = 0;
    }
}

SnapshotMap detachLoadedImages() {
    SnapshotMap map;
    for (GameImage g : values()) {
        State& s = S(g);
        destroyBeatmapSkinImage(g);
        if (s.defaultImage || !s.defaultImages.empty() || s.absentInSkin) {
            Snapshot snap;
            snap.defaultImage = s.defaultImage;
            snap.defaultImages = s.defaultImages;
            snap.isSkinned = s.isSkinned;
            snap.absentInSkin = s.absentInSkin;
            snap.rawWidth = s.rawWidth;
            snap.rawHeight = s.rawHeight;
            map.emplace(g, std::move(snap));
        }
        s.defaultImage = nullptr;
        s.defaultImages.clear();
        s.isSkinned = false;
        s.absentInSkin = false;
        s.rawWidth = s.rawHeight = 0;
    }
    return map;
}

void attachSnapshots(const SnapshotMap& map) {
    for (const auto& e : map) {
        State& s = S(e.first);
        s.defaultImage = e.second.defaultImage;
        s.defaultImages = e.second.defaultImages;
        s.isSkinned = e.second.isSkinned;
        s.absentInSkin = e.second.absentInSkin;
        s.rawWidth = e.second.rawWidth;
        s.rawHeight = e.second.rawHeight;
    }
}

void destroySnapshots(SnapshotMap& map) {
    for (auto& e : map) {
        Snapshot& s = e.second;
        try {
            if (s.defaultImage && !s.defaultImage->isDestroyed()) s.defaultImage->destroy();
            for (auto& i : s.defaultImages) {
                if (i && !i->isDestroyed()) i->destroy();
            }
        } catch (const std::exception&) {
            // nothing useful to do: the texture is gone either way (Java ignores it too)
        }
    }
    map.clear();
}

void loadPreloadImages() {
    Image::beginBatch();
    try {
        for (GameImage g : values()) {
            if (I(g).preload) setDefaultImage(g);
        }
    } catch (...) {
        Image::clearPrefetch();
        Image::endBatch();
        throw;
    }
    Image::clearPrefetch();
    Image::endBatch();
}

int count() { return gameimage_detail::kGameImageCount; }

const std::vector<GameImage>& values() {
    static const std::vector<GameImage> v = [] {
        std::vector<GameImage> out;
        for (int i = 0; i < gameimage_detail::kGameImageCount; i++) out.push_back(static_cast<GameImage>(i));
        return out;
    }();
    return v;
}

const char* name(GameImage g) { return I(g).name; }

bool valueOf(const std::string& n, GameImage& out) {
    for (int i = 0; i < gameimage_detail::kGameImageCount; i++) {
        if (n == gameimage_detail::kGameImageTable[i].name) {
            out = static_cast<GameImage>(i);
            return true;
        }
    }
    return false;
}

const gameimage_detail::GameImageInfo& info(GameImage g) { return I(g); }

Image scaleMin1px(const Image& img, float scale) {
    int w = java::max(1, java::toInt(static_cast<double>(static_cast<float>(img.getWidth()) * scale)));
    int h = java::max(1, java::toInt(static_cast<double>(static_cast<float>(img.getHeight()) * scale)));
    return img.getScaledCopy(static_cast<float>(w), static_cast<float>(h));
}

// ---- per image ---------------------------------------------------------------------------------------------------------------
bool isBeatmapSkinnable(GameImage g) { return I(g).beatmapSkinnable; }
bool isPreload(GameImage g) { return I(g).preload; }

std::shared_ptr<Image> getImage(GameImage g) {
    setDefaultImage(g);
    State& s = S(g);
    if (I(g).bgLocked)  // may be null: the skin providing scorebar-bg doesn't have this part
        return hasBeatmapSkinImage(GameImage::SCOREBAR_BG) ? s.skinImage : s.defaultImage;
    return s.skinImage ? s.skinImage : s.defaultImage;
}

std::shared_ptr<Image> getDefaultImage(GameImage g) {
    setDefaultImage(g);
    return S(g).defaultImage;
}

const std::vector<std::shared_ptr<Image>>& getImages(GameImage g) {
    setDefaultImage(g);
    State& s = S(g);
    if (I(g).bgLocked) return hasBeatmapSkinImage(GameImage::SCOREBAR_BG) ? s.skinImages : s.defaultImages;
    return !s.skinImages.empty() ? s.skinImages : s.defaultImages;
}

const std::vector<std::shared_ptr<Image>>& getDefaultImages(GameImage g) {
    setDefaultImage(g);
    return S(g).defaultImages;
}

std::vector<std::shared_ptr<Image>> getFrames(GameImage g) {
    if (hasBeatmapSkinImages(g)) return getImages(g);
    if (hasBeatmapSkinImage(g)) return {getImage(g)};
    const auto& frames = getImages(g);
    if (!frames.empty()) return frames;
    auto single = getImage(g);
    if (single) return {single};
    return {};
}

std::shared_ptr<Image> getFirstFrame(GameImage g) {
    auto frames = getFrames(g);
    return frames.empty() ? nullptr : frames[0];
}

std::shared_ptr<Image> getPlainImage(GameImage g) {
    auto img = getImage(g);
    if (img) return img;
    Registry& r = R();
    State& s = S(g);
    const SkinInfo* skin = r.skin.get();
    if (!s.builtinPlain || s.builtinPlainSkin != skin || s.builtinPlainScale != getUIscale()) {
        auto raw = loadImageSingle(g, "");
        if (!raw) return nullptr;
        if (r.containerHeight == 0) throw std::logic_error("GameImage::init() was not called (Java: ArithmeticException)");
        int unscaledWidth = UNSCALED_HEIGHT * r.containerWidth / r.containerHeight;
        s.builtinPlain = finishImage(processSub(g, raw, unscaledWidth, UNSCALED_HEIGHT), I(g).scalesWithUI);
        s.builtinPlainSkin = skin;
        s.builtinPlainScale = getUIscale();
    }
    return s.builtinPlain;
}

Animation getAnimation(GameImage g) {
    std::vector<std::shared_ptr<Image>> images = getImages(g);
    if (images.empty()) images = {getImage(g)};
    if (!images[0] && images.size() == 1) {
        Log::error("GameImage", std::string("No image for animation '") + I(g).name + "'.");
        return Animation();
    }
    int fps = R().skin ? R().skin->getAnimationFramerate() : -1;
    int n = static_cast<int>(images.size());
    if (fps == -1) fps = n;
    if (fps <= 0) fps = n;  // Java: ArithmeticException (0) / negative duration; not reachable with a sane skin
    return Animation(images, 1000 / fps);
}

Animation getAnimation(GameImage g, int duration) {
    std::vector<std::shared_ptr<Image>> images = getImages(g);
    if (images.empty()) images = {getImage(g)};
    if (!images[0] && images.size() == 1) {
        Log::error("GameImage", std::string("No image for animation '") + I(g).name + "'.");
        return Animation();
    }
    return Animation(images, duration);
}

bool exists(GameImage g) { return getImage(g) != nullptr || !getImages(g).empty(); }

void setImage(GameImage g, std::shared_ptr<Image> img) {
    State& s = S(g);
    if (s.skinImage)
        s.skinImage = std::move(img);
    else
        s.defaultImage = std::move(img);
}

void setImage(GameImage g, std::shared_ptr<Image> img, int index) {
    State& s = S(g);
    if (index < 0) return;
    size_t i = static_cast<size_t>(index);
    if (!s.skinImages.empty()) {
        if (i < s.skinImages.size()) s.skinImages[i] = std::move(img);
    } else {
        if (i < s.defaultImages.size()) s.defaultImages[i] = std::move(img);
    }
}

void setDefaultImage(GameImage g) {
    State& s = S(g);
    const GameImageInfo& info = I(g);
    const SkinInfo* skin = R().skin.get();
    if (s.defaultImage || !s.defaultImages.empty() || s.absentInSkin || !skin) return;

    // check for multiple images first, then single images
    std::string skinDir = skin->getDirectory();

    // scorebar parts: only the skin that provides scorebar-bg may provide them
    // (if it doesn't, the bg and all of its parts come from the default resources)
    bool skinOnly = false;
    if (info.bgLocked && !skinDir.empty()) {
        setDefaultImage(GameImage::SCOREBAR_BG);
        skinOnly = S(GameImage::SCOREBAR_BG).isSkinned;
    }

    // try to load from skin directory
    if (!skinDir.empty() && info.allowsSkin && (!info.bgLocked || skinOnly)) {
        if (loadDefaultFrom(g, skinDir)) {
            s.isSkinned = true;
            process(g);
            return;
        }
    }

    // the skin provides scorebar-bg but not this part: it is not drawn
    if (skinOnly) {
        s.absentInSkin = true;
        return;
    }

    // try to load default image
    if (loadDefaultFrom(g, "")) {
        s.isSkinned = false;
        process(g);
        return;
    }

    // scorebar parts that the default skin doesn't have and images that only exist in some skins are optional
    if (info.bgLocked || info.optional) {
        s.absentInSkin = true;
        return;
    }

    Log::error("GameImage", std::string("Could not find default image '") + info.filename + "'.");
}

bool setBeatmapSkinImage(GameImage g, const std::string& dir) {
    if (dir.empty() || !I(g).allowsSkin) return false;
    State& s = S(g);

    // destroy the existing images, if any
    destroyBeatmapSkinImage(g);

    // beatmap skins disabled
    if (beatmapSkinIgnored()) return false;

    // try to load multiple images
    s.skinImages = loadImageArray(g, dir);
    if (!s.skinImages.empty()) {
        if (keepsPlainImage(g)) s.skinImage = loadImageSingle(g, dir);  // may be null
        process(g);
        return true;
    }

    // try to load a single image
    s.skinImage = loadImageSingle(g, dir);
    if (s.skinImage) {
        process(g);
        return true;
    }
    return false;
}

void destroyBeatmapSkinImage(GameImage g) {
    State& s = S(g);
    if (!s.skinImage && s.skinImages.empty()) return;
    try {
        if (s.skinImage) {
            if (!s.skinImage->isDestroyed()) s.skinImage->destroy();
            s.skinImage = nullptr;
        }
        if (!s.skinImages.empty()) {
            for (auto& im : s.skinImages) {
                if (im && !im->isDestroyed()) im->destroy();
            }
            s.skinImages.clear();
        }
    } catch (const std::exception& e) {
        Log::error("GameImage", std::string("Failed to destroy beatmap skin images for '") + I(g).name + "'.", e);
    }
}

void dispose(GameImage g) {
    State& s = S(g);
    if (s.skinImage) {
        if (!s.skinImage->isDestroyed()) s.skinImage->destroy();
        s.skinImage = nullptr;
    }
    if (s.defaultImage) {
        if (!s.defaultImage->isDestroyed()) s.defaultImage->destroy();
        s.defaultImage = nullptr;
    }
    s.absentInSkin = false;
    s.rawWidth = s.rawHeight = 0;
}

int getRawWidth(GameImage g) { return S(g).rawWidth; }
int getRawHeight(GameImage g) { return S(g).rawHeight; }
bool hasGameSkinImage(GameImage g) { return S(g).isSkinned; }
bool hasBeatmapSkinImage(GameImage g) { return S(g).skinImage && !S(g).skinImage->isDestroyed(); }
bool hasBeatmapSkinImages(GameImage g) { return !S(g).skinImages.empty(); }
bool isProvidedByBeatmapSkin(GameImage g) { return hasBeatmapSkinImage(g); }

bool isProvidedByGameSkin(GameImage g) {
    setDefaultImage(g);
    State& s = S(g);
    return s.isSkinned && (s.defaultImage || !s.defaultImages.empty());
}

}  // namespace gameimage
}  // namespace opsu
