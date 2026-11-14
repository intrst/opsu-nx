#include "opsu/gfx/ShelfPacker.h"

#include <algorithm>

namespace opsu {

void ShelfPacker::reset() {
    pages_ = 0;
    x_ = y_ = rowH_ = 0;
}

bool ShelfPacker::pack(int w, int h, Slot& out) {
    if (w < 1 || h < 1 || w > pageW_ || h > pageH_) return false;
    bool fresh = false;
    if (pages_ == 0 || y_ + h > pageH_) {
        fresh = true;
    } else if (x_ + w > pageW_) {  // next row
        x_ = 0;
        y_ += rowH_ + pad_;
        rowH_ = 0;
        if (y_ + h > pageH_) fresh = true;
    }
    if (fresh) {
        pages_++;
        x_ = y_ = rowH_ = 0;
    }
    rowH_ = std::max(rowH_, h);
    out.page = pages_ - 1;
    out.x = x_;
    out.y = y_;
    out.newPage = fresh;
    x_ += w + pad_;
    return true;
}

}  // namespace opsu
