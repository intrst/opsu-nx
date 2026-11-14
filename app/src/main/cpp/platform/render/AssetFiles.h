#pragma once
// Reading files for the renderer: from the file system or from the APK assets (AAssetManager), plus the stb_image decoder.
// Shared by the GLES backend and a later Vulkan backend. Android only.
#include <android/asset_manager.h>

#include <cstdint>
#include <string>
#include <vector>

#include "opsu/gfx/IRenderBackend.h"

namespace opsu {
namespace render {

/** Must be set before assets are read (android_app->activity->assetManager). */
void setAssetManager(AAssetManager* mgr);
AAssetManager* assetManager();

/**
 * Absolute paths (start with '/') are read from the file system. Anything else is an APK asset path like "res/cursor.png";
 * if that asset does not exist the path is tried as a file as well. false if nothing could be read.
 */
bool readFileOrAsset(const std::string& path, std::vector<uint8_t>& out);

/** IImageDecoder over stb_image: PNG/JPEG/BMP/GIF/TGA... to RGBA8, row 0 = top. */
class StbImageDecoder : public IImageDecoder {
public:
    bool decode(const std::string& path, Pixmap& out) override;
};

}  // namespace render
}  // namespace opsu
