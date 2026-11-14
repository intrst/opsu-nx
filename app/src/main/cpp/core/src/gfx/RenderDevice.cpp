#include "opsu/gfx/IRenderBackend.h"

namespace opsu {

namespace {
std::shared_ptr<IRenderDevice>& slot() {
    static std::shared_ptr<IRenderDevice> d;
    return d;
}
}  // namespace

void setRenderDevice(std::shared_ptr<IRenderDevice> device) { slot() = std::move(device); }
IRenderDevice* renderDevice() { return slot().get(); }

}  // namespace opsu
