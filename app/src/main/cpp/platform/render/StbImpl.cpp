// The ONE translation unit that holds the stb implementations (shared by the GLES and a later Vulkan backend).
// Other files include the headers for declarations only.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_STDIO  // files are read by AssetFiles (file system or APK asset), then decoded from memory
#include "../../third_party/stb/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"
