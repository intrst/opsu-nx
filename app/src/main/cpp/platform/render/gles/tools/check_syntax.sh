#!/bin/sh
# Desktop syntax check of the GLES backend against minimal stub headers (no NDK needed):  sh check_syntax.sh
# Expect: no output. The real build uses the NDK headers; the stubs only declare what this code calls.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
CPP=$(cd "$HERE/../../../.." && pwd)    # app/src/main/cpp
for f in "$CPP"/platform/render/gles/*.cpp "$CPP"/platform/render/AssetFiles.cpp "$CPP"/platform/render/StbFontFace.cpp "$CPP"/platform/render/StbImpl.cpp; do
    g++ -std=c++17 -Wall -Wextra -ffp-contract=off -fsyntax-only -I"$HERE/stubs" -I"$CPP/core/include" "$f"
done
