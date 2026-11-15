# T9a: renderer interface + OpenGL ES 3.0 backend

Status: **DONE (all 4 milestones), not yet run on a device.** Everything that can be checked without a GPU/NDK passes (see Tests).
The device checklist at the end is the remaining acceptance step.

## Design (differs a little from the brief)
`opsu::Graphics`, `Image`, `Font`/`UnicodeFont`, `RenderTarget`, `Animation`, `SpriteSheet` are CONCRETE pure classes in `core/` (names and
behaviour follow the Java `fake` layer). They talk to the platform through a small contract, `core/include/opsu/gfx/IRenderBackend.h`:
`IRenderBackend` (triangle lists, draw mode, scissor, target, clear, flush), `IRenderDevice` (textures, render targets, image decoder, font
faces), `ITexture`, `IRenderTarget`, `IImageDecoder`, `IFontFace`. No GL/EGL/Vulkan/Android type appears in `core/`. The Vulkan backend (T9b)
only implements that contract; transforms, tessellation, text layout, atlas packing, hue shift and the size cap are shared and tested on the desktop.
- Colour: reuses `opsu::Color` (`util/Color.h`); named constants in `gfx/Colors.h` (`colors::white` ...).
- The Graphics transform is applied on the CPU per vertex (a transform change never flushes the batch); vertices reach the backend in game coordinates, y down.
- Lines/rects/ovals are quads of `setLineWidth` SCREEN pixels (GL_LINES widths > 1 are unsupported on most GLES drivers).
- RenderTarget: its own pixel size is the game space (decided with you); `setVirtualSize(w, h)` is there for other cases.

## Done
**core/ (pure, M1):** DrawMode table (modes 1-4 with the Java masks/blends), Transform2D + stack, ViewportMap + `computeScissor` (libGDX rules incl. "under 1 px = no clip"),
ShelfPacker, UTF-8 + text layout (Java `DynamicFreeTypeFont` rules), TextureRegion UVs (render-target textures are upright), Graphics (state, sprites, pixel snap, clip, shapes,
gradients, arcs, additive shapes, text, targets), Font (on-demand 512x512 glyph pages, fallback font, bold, scaled variants), Image + ImageManager (cache, refcount, atlas pages,
hue shift, size cap, context-loss restore, sub/scaled copies, flip, getAlphaAt/getColor), RenderTarget (bind/unbind restores target, viewport and clip), Animation, SpriteSheet.
**platform/render/ (M2-M4):** `third_party/stb/` (stb_image 2.32, stb_truetype 1.28, unchanged), `StbImpl.cpp` (the one implementation unit, shared with Vulkan), `AssetFiles` (file system or APK
asset + stb decoder), `StbFontFace` (IFontFace over stb_truetype, FreeType-style metrics), and `gles/`: `GlesShaders.h` (all shaders, one file), `GlesBackend` (one program, one dynamic VBO,
8192-quad batches, flush on texture/mode/scissor/target change), `GlesDevice` (textures, FBOs, context-loss bookkeeping), `GlesWindow` (EGL ES 3.0 RGBA8888,
`init/onResize/beginFrame/endFrame/term/graphics/debugDrawTestScene`).
**Context loss:** every GL name carries the generation of its context. `term()` / `EGL_CONTEXT_LOST` bump it with no GL calls and destroy surface and context; the next `init()` makes a new
context and refills every live texture through its owner's restorer (atlas pages re-decode their files, glyph pages re-upload their CPU copy); FBOs return empty. `ITexture`, `Image`, `Font`,
`RenderTarget` objects stay valid; drawing is silently skipped while there is no context.
**Test scene (`debugDrawTestScene`):** rotating/scaled/tinted/pulsing sprite; rotated+translated group (push/pop); clipped bar + sprite; two additive sprites; bold title and mixed Latin/CJK/Hangul text
with line break; pixel-snapped scaled text; render-target smoke test (a 256x128 target gets a gradient, frame, oval, rotating cursor and text, then its texture is drawn plain and rotated/scaled); on-screen fps counter.
**Changes in M4:** render-target smoke test in the scene and the mock test; removed the unused private field `started_` from `gfx/Animation.h`.
Bug found by the mock test in M2 and fixed in core: `Graphics` did not hand pending vertices to the backend before `setDrawMode`/`setClip`/`clearClip` (regression test in `test_gfx_t9a.cpp`).

## WIP / not done
- Nothing is run on a device yet (no NDK/GPU where this was built): shaders, EGL setup and the real GL driver behaviour are only covered by the syntax check and the fake-GL test.
- Not ported: `Image.prefetch/clearPrefetch/beginBatch/endBatch` (no-ops: uploads are sub-rectangles, no background decode); `Animation(String)` (throws in the Java).
- Differences from the Java, all on purpose: sub-image rounding uses the texels-per-unit ratio on both axes (Java used dy for x, a typo); `setFlipped` is per Image (Java shared the region with copies);
  `fillArc` also accepts w != h (Java throws); `fillRoundRect/drawRoundRect` draw plain rects like the Java; glyph pages are filtered linearly (Java: nearest, same at 1:1); text is UTF-8 code points
  (Java iterates UTF-16 units); a null texture region is ignored instead of throwing; unhinted glyphs (below 16 px advances differ slightly from the Java's hinted FreeType; accepted); no kerning (as the Java).
- Vulkan: only the contract is prepared (T9b). `StbImpl.cpp`, `AssetFiles`, `StbFontFace`, `GlesShaders.h` (GLSL) are written to be reused.

## Tests (all pass)
```
cd app/src/main/cpp/core && g++ -std=c++17 -ffp-contract=off -Wall -Wextra -fsanitize=address,undefined -Iinclude \
    tests/test_gfx_t9a.cpp src/gfx/*.cpp src/util/*.cpp -o /tmp/t9a && /tmp/t9a          # test_gfx_t9a: all passed
sh app/src/main/cpp/platform/render/gles/tools/check_syntax.sh                           # no output = every platform/render file compiles (stub EGL/GLES/android headers)
sh app/src/main/cpp/platform/render/gles/tools/run_mock_test.sh                          # test_gles_mock: all passed (fake GL, real Exo2/DroidSansFallback files)
```
- `test_gfx_t9a`: draw modes, transforms, scissor maths, packer, UTF-8/layout, UVs, Graphics through a recording backend (batching order, state-change ordering, clip, snap, lines in screen pixels,
  arcs/ovals), Font (metrics, fallback, pages, destroy), ImageManager/Image (atlas, refcount, cap, hue, restore, sub/scaled), RenderTarget, Animation.
- `test_gles_mock`: 1000 sprites = 1 draw call; texture/solid/mode/scissor changes split batches; exact glColorMask/glBlendFunc per draw mode; context loss + recovery (names die, restorer refills,
  stale objects delete nothing, silent while lost); FBO bind/unbind/rebuild/viewport; the scene's render-target sequence; real fonts through stb_truetype incl. fallback and page refill.
- `tools/test_gles_mock.cc` has the extension .cc on purpose: CMake globs `platform/**.cpp`, and `tools/stubs/` headers are never on the app's include path.

## Integrator edits (exact text; I did not apply them)
1. `app/src/main/cpp/CMakeLists.txt`, last line: `target_link_libraries(opsu opsu_core app_glue bass bass_fx android log EGL GLESv3)`.
   (`platform/render/**.cpp` incl. `StbImpl.cpp` is picked up by the existing glob; the stb headers are included by relative path.)
2. `core/CMakeLists.txt`: add `test_gfx_t9a` to the test `foreach` list and `add_test(NAME opsu_gfx_t9a COMMAND opsu_test_gfx_t9a)` (`src/gfx/*.cpp` is already globbed).
3. `AndroidManifest.xml`: replace the Vulkan `uses-feature` with `<uses-feature android:name="android.hardware.vulkan.version" android:required="false" />` and add
   `<uses-feature android:glEsVersion="0x00030000" android:required="true" />`. On the activity add
   `android:configChanges="orientation|screenSize|smallestScreenSize|screenLayout|keyboardHidden|density"` so rotation arrives as APP_CMD_WINDOW_RESIZED (without it the window is destroyed and re-created, which also works: term() + init()).
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

## Device checklist
Run `adb logcat -s opsu-gles opsu-render`. Needs `assets/res/cursor.png`, `Exo2-Regular.ttf`, `Exo2-Bold.ttf`, `DroidSansFallback.ttf` (all present in the project).
1. Start log: `EGL 1.x`, `GL_RENDERER=... GL_VERSION=OpenGL ES 3.x`, `context created (generation N): 0 textures restored ...`, `surface WxH`.
2. Picture: dark blue gradient background.
   - top-left: an orange cursor, 3x, rotating and pulsing in alpha;
   - right of centre: a rotating blue bar with a cursor on it and a white outline (outline stays 1-2 px thick while rotating);
   - bottom-left: a green bar and a moving cursor, cut off exactly at the white clip box;
   - bottom-right: two overlapping additive sprites (orange + blue), brighter where they overlap.
3. Text: top-centre white bold "Lumina English Academy"; middle-left light-blue `Hello, World! éñü  日本語  한글` with `second line (lineHeight)` below (sharp, one baseline); orange "scaled 1.5x, pixel snapped" (crisp, not blurry);
   top-left green `fps 60  calls N`.
4. Render target (top-right): a 256x128 panel with red-to-blue gradient, white frame, oval, a cursor spinning the opposite way, and the text "offscreen target" reading UPRIGHT (if it is upside down, `rowZeroIsBottom` handling is wrong);
   next to it on the left a smaller copy tilted 12 degrees, same content, also upright.
5. Batching: logcat `fps=60.0 drawCalls=N` once a second. The scene deliberately alternates solid shapes, sprites and text, so expect roughly 25-45; hundreds means batching is broken.
6. Press Home and return: `context lost`, then `context created (generation N+k): <n> textures restored`, same picture (the render-target panel is redrawn every frame so it comes back too).
   Rotate the phone: `surface HxW` and the picture re-fits without stretching wrongly.
7. Black screen? Look for `shader compile failed`, `program link failed`, `FBO ... incomplete`, `decode failed: res/cursor.png`, `font not found`. `GlesWindow::setDebugChecks(true)` once logs GL errors (stalls the pipeline, debug only).
   If CJK shows boxes, `DroidSansFallback.ttf` is missing from `assets/res`.
