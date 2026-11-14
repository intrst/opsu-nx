#include "opsu/kww/UselessUtils.h"
#include <cstdio>

namespace opsu {
namespace kww {

std::string screenshotFileName(const std::tm& t) {
    // SimpleDateFormat("yyyyMMdd_HHmmss"): yyyy is zero-padded to at least 4 digits, the rest to 2.
    char buf[64];
    std::snprintf(buf, sizeof buf, "screenshot_%04d%02d%02d_%02d%02d%02d.png",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

std::string screenshotFileName(std::time_t now) {
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    return screenshotFileName(t);
}

void makeOpaque(uint8_t* rgba, std::size_t sizeBytes) {
    for (std::size_t i = 3; i < sizeBytes; i += 4)
        rgba[i] = 255;
}

}  // namespace kww
}  // namespace opsu
