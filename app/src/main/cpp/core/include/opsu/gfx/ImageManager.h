#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opsu/gfx/IRenderBackend.h"
#include "opsu/gfx/ShelfPacker.h"

namespace opsu {

/**
 * The cache and atlas part of fluddokt.opsu.fake.Image: decodes image files once (per hue shift), caps huge images to
 * the texture cap, packs small ones into shared 512 x 512 atlas pages (so the backend can batch many sprites into one
 * draw call) and gives every file with >= 512 pixels its own texture. Reference counted like Image.imgmap.
 * After a lost graphics context the device calls each page's restorer, which re-decodes the files (like the Java's loadPixmap).
 * Not ported: Image.prefetch (background decoding) and beginBatch/endBatch upload batching (uploads are sub-rectangles here).
 */
class ImageManager {
public:
    struct Atlas;
    struct Entry {
        std::string key, path;
        float hueShift = 0.f;
        TextureRegion region;           ///< texels in the atlas page / own texture (after the size cap)
        int logicalW = 0, logicalH = 0; ///< size of the picture in the FILE (what Image.getWidth reports)
        int refcnt = 0;
        bool freed = false;             ///< clearAll() or the last reference went away
        std::weak_ptr<Atlas> atlas;     ///< weak: the atlas lists its entries (no reference cycle)
    };
    struct Atlas {
        std::shared_ptr<ITexture> tex;
        int w = 0, h = 0;
        bool full = false;
        std::shared_ptr<Pixmap> cpu;    ///< pixels of a page that is still being filled (null once full)
        std::vector<std::shared_ptr<Entry>> entries;
    };

    static ImageManager& instance();

    /** Loads or reuses `path` (+1 reference). null if the file cannot be decoded (the Java then uses a blank image). */
    std::shared_ptr<Entry> load(const std::string& path, float hueShift = 0.f);
    void retain(const std::shared_ptr<Entry>& e);
    /** -1 reference; the last one frees the atlas slot. */
    void release(const std::shared_ptr<Entry>& e);
    /** Image.clearAll(): free everything. Images that still point at an entry draw nothing. */
    void clearAll();

    int atlasCount() const { return static_cast<int>(atlases_.size()); }
    int entryCount() const { return static_cast<int>(map_.size()); }

    /** Longest texture side (Image.getTexCap): min(2048, max(1600, longest screen side)), also <= the GPU limit. Fixed after the first call. */
    int textureCap();
    void resetTextureCapForTests() { texCap_ = -1; }

    /** Image.shiftHue: rotates the hue of every non-grey pixel, keeps saturation, value and alpha. */
    static void shiftHue(Pixmap& p, float degrees);
    /** Image.capPixmap: a smaller copy (area average) if the longest side exceeds `cap`, else p unchanged. */
    static Pixmap capPixmap(Pixmap p, int cap);

private:
    void place(const std::shared_ptr<Entry>& e, const Pixmap& p);
    void dropIfEmpty(const std::shared_ptr<Atlas>& a);
    void restoreAtlas(Atlas& a);

    std::map<std::string, std::shared_ptr<Entry>> map_;
    std::vector<std::shared_ptr<Atlas>> atlases_;
    std::shared_ptr<Atlas> current_;
    ShelfPacker packer_{512, 512, 1};
    int texCap_ = -1;
};

}  // namespace opsu
