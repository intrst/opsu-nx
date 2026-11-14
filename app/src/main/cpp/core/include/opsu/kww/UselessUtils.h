#pragma once
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

namespace opsu {
namespace kww {

/**
 * PARTIAL port of kww.util.UselessUtils: only the backend-free parts of the screenshot code.
 *
 * Not ported (needs the graphics/audio backend, see PORTING_NOTES.md):
 *  - takeScreenshot(): frame-buffer read-back, the background thread, SoundEffect::SHUTTER, the
 *    "Screenshot saved" notification, Options.getScreenshotDir().
 *  - savePixmapTo(): PNG encoding (libGDX PixmapIO.writePNG, deflate default level) and its `flip`
 *    flag (vertical row flip, GL frame buffers are bottom-up). Do the flip in the PNG writer.
 */

/** The file name used for a screenshot: "screenshot_" + yyyyMMdd_HHmmss + ".png" (24-hour clock). */
std::string screenshotFileName(const std::tm& t);

/** Same, for an instant, formatted in the machine's local time zone (Java `new Date()` default zone). */
std::string screenshotFileName(std::time_t now);

/**
 * "This loop makes sure the whole screenshot is opaque": sets every 4th byte starting at index 3
 * (the alpha of each RGBA pixel) to 255. Only whole pixels are touched, a trailing partial pixel
 * is left alone, like the Java loop (`for (i = 3; i < size; i += 4)`).
 */
void makeOpaque(uint8_t* rgba, std::size_t sizeBytes);

}  // namespace kww
}  // namespace opsu
