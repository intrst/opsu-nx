# T9a: renderer interface + OpenGL ES 3.0 backend

Status: **WIP, milestones 1-2 of 4 done** (M1 interface + pure code; M2 EGL window + GLES sprite batch). Fonts (M3) and render target/test scene polish (M4) follow.

## Design (please read, it differs a little from the brief)
`opsu::Graphics`, `Image`, `Font`, `RenderTarget`, `Animation`, `SpriteSheet` are CONCRETE pure classes in `core/` (names and
behaviour follow the Java `fake` layer). They talk to the platform through a small contract in
`core/include/opsu/gfx/IRenderBackend.h`: `IRenderBackend` (triangle lists, draw mode, scissor, target, clear, flush),
`IRenderDevice` (textures, render targets, image decoder, font faces), `ITexture`, `IRenderTarget`, `IImageDecoder`, `IFontFace`.
GLES (M2-4) and the later Vulkan backend (T9b) only implement that contract; transforms, tessellation, text layout, atlas packing,
hue shift, size cap are shared and tested on the desktop. No GL type appears in `core/`.
- Colour: reuses the existing `opsu::Color` (`util/Color.h`); named constants are in `gfx/Colors.h` (`colors::white` ...).
- Transform is applied on the CPU per vertex, so a transform change never flushes the batch. Vertices reach the backend in game coordinates.
- Lines/rects/ovals are quads of `setLineWidth` SCREEN pixels (GL_LINES widths > 1 are unsupported on most GLES drivers).

## Done in milestone 1 (all in core/, tests pass)
DrawMode table (NORMAL/ALPHA_MAP/ALPHA_BLEND/ADD, numbers and masks checked against `Graphics.setDrawMode`), Transform2D + stack
(rotate-about-point, post-multiply like Matrix4), ViewportMap + `computeScissor` (libGDX ScissorStack rules, incl. the "under 1 px = no clip" quirk),
ShelfPacker (glyph pages / image atlases), UTF-8 + text layout (Java `DynamicFreeTypeFont` rules), TextureRegion UVs,
Graphics (state, sprites, pixel snap, clip, shapes, additive shapes, gradients, arcs, text, targets), Font (on-demand 512x512 glyph pages,
fallback font, bold, scaled variants), Image + ImageManager (cache, refcount, atlas pages, hue shift, size cap, context-loss restore, sub/scaled copies,
flip, getAlphaAt/getColor), RenderTarget (bind/unbind restores target, viewport, clip), Animation, SpriteSheet.

## Not ported / differences (so far)
- `Image.prefetch/clearPrefetch/beginBatch/endBatch` are no-ops (uploads are sub-rectangles; no background decode).
- Sub-image rounding uses the intended texels-per-unit ratio on both axes (the Java used dy for x, a typo).
- `setFlipped` is per Image (Java shared the TextureRegion with copies). `Animation(String)` not ported (throws in Java).
- `fillArc` also accepts w != h (Java throws). `fillRoundRect/drawRoundRect` draw plain rects, as in the Java (TODO there).
- Glyph pages use linear filtering (Java: libGDX default nearest). Identical at 1:1, smoother when scaled.
- Text is UTF-8 code points; Java iterates UTF-16 units (astral characters were two "missing" glyphs).
- Kerning: none (the Java has it commented out). Hinting: see M3 (stb_truetype is unhinted; Java hints below 16 px).

## Open question for you (non-blocking)
Java keeps the SCREEN camera while an FBO is bound (screen space squeezed into the FBO). Here a `RenderTarget` uses its own pixel size as the game
space; `RenderTarget::setVirtualSize(w, h)` gives the Java behaviour. Which one should the future SliderBodyRenderer port assume?

## Tests
`cd app/src/main/cpp/core && g++ -std=c++17 -ffp-contract=off -Wall -Wextra -fsanitize=address,undefined -Iinclude tests/test_gfx_t9a.cpp src/gfx/*.cpp src/util/*.cpp -o /tmp/t9a && /tmp/t9a`
-> `test_gfx_t9a: all passed`, no warnings, ASan/UBSan/LSan clean.

## Done in milestone 2 (GLES backend, platform/render/)
- `third_party/stb/stb_image.h` (v2.32), `stb_truetype.h` (v1.28) vendored unchanged. The single implementation unit is `platform/render/StbImpl.cpp` (stb_image + stb_truetype, shared with a later Vulkan backend, as agreed).
- `platform/render/AssetFiles.{h,cpp}`: `readFileOrAsset` (absolute path = file system, else APK asset like `res/cursor.png`, fallback file) and `StbImageDecoder` (RGBA8).
- `platform/render/gles/`: `GlesShaders.h` (ALL shader source, GLSL ES 3.00, one program, solid shapes sample a 1x1 white texture), `GlesBackend` (one dynamic VBO, 8192-quad batches, flush on texture/mode/scissor/target change, orphaned buffer upload, no glGetError unless `GlesWindow::setDebugChecks(true)`),
  `GlesDevice` (textures, FBO render targets, context-loss bookkeeping), `GlesWindow` (EGL ES 3.0 RGBA8888 config, `init/onResize/beginFrame/endFrame/term/graphics/debugDrawTestScene`).
- Context loss: every GL name carries the "generation" of the context it was made in. `term()`/`EGL_CONTEXT_LOST` bump it without GL calls, destroy surface AND context; the next `init()` makes a new context, then refills each live texture through its restorer (atlas pages re-decode their files, glyph pages re-upload their CPU copy) and rebuilds FBOs (empty). Objects (ITexture, RenderTarget, Font, Image) stay valid across it; draws are silently skipped while there is no context.
- `debugDrawTestScene` (sprites part): cursor.png rotated + scaled 3x + orange tint + pulsing alpha; rotated/translated group (push/pop); clipped bar and sprite (`setClip`); two overlapping additive (`MODE_ADD`) sprites; fps + draw-call count to logcat once a second. Text, the render-target smoke test and the on-screen fps counter come with M3/M4.
- BUG FOUND AND FIXED in M1 code (core, files re-sent): `Graphics` did not hand vertices collected so far to the backend before `setDrawMode`/`setClip`/`clearClip`, so they would have been drawn with the new state. Found by the new mock-GL test; regression test added to `test_gfx_t9a.cpp`.

## Tests (M2)
- `sh app/src/main/cpp/platform/render/gles/tools/check_syntax.sh`: every platform/render file with `g++ -std=c++17 -Wall -Wextra -fsyntax-only` against minimal stub `<EGL/egl.h>`, `<GLES3/gl3.h>`, `<android/*.h>` (in `tools/stubs/`, never used by the app build). Clean.
- `sh app/src/main/cpp/platform/render/gles/tools/run_mock_test.sh`: GlesBackend + GlesDevice + the core gfx code against a FAKE GL that records calls (ASan/UBSan). Checks: 1000 sprites = 1 draw call; texture/solid/mode/scissor changes split batches; the four draw modes set the exact glColorMask/glBlendFunc; context loss (names die, restorer refills, old objects delete nothing, silent while lost); FBO bind/unbind/rebuild; viewport per target. Passes.
- `tools/test_gles_mock.cc` has extension .cc on purpose: CMake globs `platform/*.cpp`. NOT run on a device (no NDK/GPU here).

## Integrator edits (exact text; I did not apply them)
1. `app/src/main/cpp/CMakeLists.txt`, last line: `target_link_libraries(opsu opsu_core app_glue bass bass_fx android log EGL GLESv3)`.
   (`platform/render/**.cpp` incl. `StbImpl.cpp` is picked up by the existing glob. The stb headers are included by relative path, no include dir needed.)
2. `core/CMakeLists.txt`: add `test_gfx_t9a` to the test `foreach` list and `add_test(NAME opsu_gfx_t9a COMMAND opsu_test_gfx_t9a)`.
3. `AndroidManifest.xml`: replace the Vulkan `uses-feature` with
   `<uses-feature android:name="android.hardware.vulkan.version" android:required="false" />` and add
   `<uses-feature android:glEsVersion="0x00030000" android:required="true" />`.
   On the activity add `android:configChanges="orientation|screenSize|smallestScreenSize|screenLayout|keyboardHidden|density"` so rotation arrives as APP_CMD_WINDOW_RESIZED instead of recreating the activity (without it the window is destroyed and re-created, which also works: term() + init()).
4. `main.cpp` (renderer test; keep the audio lines you have):
```cpp
#include "platform/render/gles/GlesWindow.h"
#include <chrono>

static opsu::gles::GlesWindow g_window;

static void onCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:     if (app->window) g_window.init(app->window); break;
        case APP_CMD_TERM_WINDOW:     g_window.term(); break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:  g_window.onResize(); break;
    }
}

// in android_main, before the loop:
g_window.setAssetManager(app->activity->assetManager);
app->onAppCmd = onCmd;
auto last = std::chrono::steady_clock::now();

// the loop: poll without waiting while there is a surface (eglSwapBuffers with swap interval 1 paces the frames)
while (!app->destroyRequested) {
    int events; android_poll_source* src;
    while (ALooper_pollOnce(g_window.hasSurface() ? 0 : 100, nullptr, &events, reinterpret_cast<void**>(&src)) >= 0)
        if (src) src->process(app, src);
    BassAudio::pumpEvents();
    if (g_window.hasSurface() && g_window.beginFrame()) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        g_window.debugDrawTestScene(app->activity->assetManager, dt);
        g_window.endFrame();
    }
}
```

## Device checklist (M2: sprites)
`adb logcat -s opsu-gles opsu-render`
1. Start: `EGL 1.x`, `GL_RENDERER=... GL_VERSION=OpenGL ES 3.x`, `context created (generation N): 0 textures restored ...`, `surface WxH`.
2. Screen: dark blue gradient; top-left an orange rotating, pulsing cursor (3x); right a blue bar rotating with a cursor on it and a white outline; bottom-left a green bar and a moving cursor cut off at the white clip box; bottom-right two overlapping additive sprites (orange + blue), brighter where they overlap.
3. Logcat once a second: `fps=60.0 drawCalls=<small number, expect ~10 or less>` (NOT hundreds).
4. Press Home, return: `context lost`, then `context created (generation N+k): <n> textures restored` and the same picture. Rotate the phone: `surface HxW`, picture re-fits, nothing stretches wrongly.
5. If it is black: look for `shader compile failed`, `program link failed`, `FBO ... incomplete`, `decode failed: res/cursor.png`. Call `GlesWindow::setDebugChecks(true)` once to log GL errors.

## Still to do
M3: stb_truetype `IFontFace` (+ Exo2/DroidSansFallback test text, fps on screen). M4: render-target smoke test in the scene, final notes. Until M3 `loadFontFace` returns null (logged), so `Font` draws nothing.
