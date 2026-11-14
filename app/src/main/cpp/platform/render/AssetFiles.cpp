#include "AssetFiles.h"

#include <android/log.h>

#include <cstdio>

#include "../../third_party/stb/stb_image.h"

#define LOG_TAG "opsu-render"

namespace opsu {
namespace render {

namespace {
AAssetManager* g_mgr = nullptr;

bool readAsset(const std::string& path, std::vector<uint8_t>& out) {
    if (!g_mgr) return false;
    AAsset* a = AAssetManager_open(g_mgr, path.c_str(), AASSET_MODE_STREAMING);
    if (!a) return false;
    const off_t len = AAsset_getLength(a);
    out.resize(static_cast<size_t>(len));
    size_t got = 0;
    while (got < out.size()) {
        const int r = AAsset_read(a, out.data() + got, out.size() - got);
        if (r <= 0) break;
        got += static_cast<size_t>(r);
    }
    AAsset_close(a);
    return got == out.size();
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len < 0) { std::fclose(f); return false; }
    out.resize(static_cast<size_t>(len));
    const size_t got = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}
}  // namespace

void setAssetManager(AAssetManager* mgr) { g_mgr = mgr; }
AAssetManager* assetManager() { return g_mgr; }

bool readFileOrAsset(const std::string& path, std::vector<uint8_t>& out) {
    if (path.empty()) return false;
    if (path[0] == '/') return readFile(path, out);
    return readAsset(path, out) || readFile(path, out);
}

bool StbImageDecoder::decode(const std::string& path, Pixmap& out) {
    std::vector<uint8_t> bytes;
    if (!readFileOrAsset(path, bytes) || bytes.empty()) return false;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &n, 4);
    if (!px) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "decode failed: %s (%s)", path.c_str(), stbi_failure_reason());
        return false;
    }
    out.w = w;
    out.h = h;
    out.rgba.assign(px, px + static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
    stbi_image_free(px);
    return true;
}

}  // namespace render
}  // namespace opsu
