#pragma once

namespace opsu {

/**
 * Shelf packer for atlas pages (glyph pages and image atlases): rows of rectangles from left to right, a new row
 * when the next one does not fit the width, a new page when it does not fit the height. Port of the packing in
 * DynamicFreeTypeFont.addChar (512 x 512 pages, 1 px padding) with one fix: the Java does not check the page height
 * again after starting a new row, so a glyph could be drawn past the bottom of the page.
 * Pure bookkeeping: pages are created by the caller when {@link Slot::newPage} is set.
 */
class ShelfPacker {
public:
    struct Slot {
        int page = 0;
        int x = 0, y = 0;
        bool newPage = false;  ///< the caller must create page `page` now
    };

    ShelfPacker(int pageW = 512, int pageH = 512, int pad = 1) : pageW_(pageW), pageH_(pageH), pad_(pad) {}

    /** False if the rectangle can never fit a page (w > pageW or h > pageH, or w/h < 1). */
    bool pack(int w, int h, Slot& out);

    int pageCount() const { return pages_; }
    int pageWidth() const { return pageW_; }
    int pageHeight() const { return pageH_; }
    /** Forget everything (all pages are gone, e.g. after the textures were destroyed). */
    void reset();

private:
    int pageW_, pageH_, pad_;
    int pages_ = 0;
    int x_ = 0, y_ = 0, rowH_ = 0;
};

}  // namespace opsu
