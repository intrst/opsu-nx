// Stub for desktop syntax checks only.
#pragma once
#include <sys/types.h>
struct AAssetManager; struct AAsset;
#define AASSET_MODE_STREAMING 2
AAsset* AAssetManager_open(AAssetManager*, const char*, int); off_t AAsset_getLength(AAsset*); int AAsset_read(AAsset*, void*, size_t); void AAsset_close(AAsset*);
