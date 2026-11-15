#!/bin/sh
# Builds and runs the host test of the GLES backend against a fake GL (needs only g++).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
CPP=$(cd "$HERE/../../../.." && pwd)
g++ -std=c++17 -Wall -Wextra -ffp-contract=off -fsanitize=address,undefined -g -DFONT_DIR=\"$CPP/../assets/res\" -I"$HERE/stubs" -I"$CPP/core/include" \
    "$HERE/test_gles_mock.cc" "$CPP/platform/render/gles/GlesBackend.cpp" "$CPP/platform/render/gles/GlesDevice.cpp" "$CPP/platform/render/AssetFiles.cpp" "$CPP/platform/render/StbFontFace.cpp" "$CPP/platform/render/StbImpl.cpp" \
    "$CPP"/core/src/gfx/*.cpp "$CPP"/core/src/util/*.cpp -o /tmp/test_gles_mock
/tmp/test_gles_mock
