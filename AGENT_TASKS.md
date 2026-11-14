# opsu!nx: agent task notes

Goal: several agents work in parallel without touching each other's files.
**Every task below owns a fixed set of files. Edit only what your task owns.**

## Rules for every agent

1. **Core stays clean.** Anything under `core/` (game logic) must not include Android, Vulkan, BASS, SQLite or JNI headers. It must build and run its tests on desktop with plain `g++ -std=c++17`.
2. **Backends go in `platform/`.** Android-specific code lives in `app/src/main/cpp/platform/<area>/` and only talks to the core through the interfaces in `core/include/opsu/`.
3. **Float rule.** Keep `-ffp-contract=off` (already set in the core CMake). Without it, slider math drifts in the last bit.
4. **Check against the Java.** The Java source is the truth (`core/src/...` in the Java core zip). Logic that uses floats or locale formatting needs a golden test against the real Java (see `core/tests/golden/README.md`).
5. **One small batch at a time:** read the Java, port a small slice, test it, then write your notes.
6. **Never leave things half done silently.** Anything partial gets a line in your notes file under "WIP".
7. **Notes:** write to `docs/agents/<TASK-ID>.md` (your own file). Do **not** edit `PORTING_NOTES.md` or any CMakeLists; the integrator (T0) merges notes.
8. **CMake:** `platform/` and `core/src` are globbed, so a new `.cpp` file is picked up without editing CMake. If you really need a CMake change, ask the integrator.
9. **No Java in the build.** The Java core is reference only and is never compiled.
10. **Do not call** `BeatmapSetList::deleteSongGroup/deleteSong` from app code until T8 is done (6 of 9 hooks are not real yet).

## Start prompt (copy, fill in the task ID)

> You are working on opsu!nx, a C++17/Vulkan Android port of opsu!. Read `AGENT_TASKS.md`, then do **task <ID>** only.
> Edit only the files in that task's "Owns" list. Follow the rules at the top. Reference Java is under `core/src/` in the Java core zip.
> Finish with a notes file `docs/agents/<ID>.md` listing what is done, what is WIP, and how you tested it.

## Layout

```
app/src/main/cpp/
  CMakeLists.txt          (T0 only)
  core/                   the pure C++ port (include/opsu, src, tests)
  platform/
    audio/ input/ log/ db/ net/ fs/      Android-only backends
  third_party/
    bass/  sqlite/                       vendored headers/sources
docs/agents/              one notes file per task
```

## Task list and order

```
T0 integrator  ──► T1 audio interface ──► T2 BASS backend
                │                     └─► T8 delete hooks
                ├─► T3 logger
                ├─► T4 touch input
                ├─► T5 SQLite + BeatmapDB
                ├─► T6 HTTP client
                ├─► T7a..T7d pure-logic ports
                └─► T9 Vulkan renderer (later)
```

T0 runs first (small). After it, T1 to T7 can run **in parallel**. T2 and T8 wait for T1's headers (T1 commits them first, see below). T9 starts last.

---

## T0: Integrator (do first, blocking)

**Owns:** `app/src/main/cpp/CMakeLists.txt`, `app/src/main/cpp/core/CMakeLists.txt`, `docs/agents/`, `PORTING_NOTES.md`.
**Goal:** put the other agent's `opsu-cpp` into `app/src/main/cpp/core/` and link it into the app.
**Steps:**
- Copy `include/`, `src/`, `tests/`, `PORTING_NOTES.md` into `core/`.
- In the core CMake, wrap the ~50 test executables and `add_test` lines in `if(NOT ANDROID)`. Keep desktop `ctest` working.
- In the app CMake: `add_subdirectory(core)`, then `file(GLOB_RECURSE PLATFORM_SRC platform/*.cpp)` and link `opsu_core` into the `opsu` library.
**Done when:** `./gradlew assembleDebug` builds and desktop `ctest` still passes.

---

## T1: Audio interface, silent backend, music and sound controllers (core, pure)

**Owns:** `core/include/opsu/audio/{IAudioBackend,AbsMusic,ClipImplementation,SilentAudioBackend,MusicController,SoundController,MultiClip}.h`, matching `core/src/audio/*.cpp`, tests.
**Java reference:** `fluddokt/opsu/fake/{AudioBackend,AbsMusic,Clip,ClipImplementation,SilentAudioBackend}.java`, `itdelatrisu/opsu/audio/{MusicController,SoundController,MultiClip,HitSound,SoundEffect}.java`. Existing C++: `SoundEffect.h`, `HitSound.h` (enums only).
**Commit the three headers first** (`IAudioBackend.h`, `AbsMusic.h`, `ClipImplementation.h`) so T2 can start. Contract:

```cpp
class AbsMusic {            // seconds are float, like Java
  virtual bool setPosition(float sec)=0; virtual void loop()=0, play()=0, pause()=0, resume()=0, stop()=0, dispose()=0;
  virtual bool playing()=0; virtual float getPosition()=0; virtual std::string getName()=0;
  virtual void setVolume(float)=0; virtual void setPitch(float)=0;
  virtual bool getFFT(float* out256) { return false; }      // 256 bins, 0..1
  virtual bool getLevels(float out[2]) { return false; }    // peak L/R
  virtual void setLowPass(float hz) {}                      // >= 20000 = remove the filter
  std::function<void(AbsMusic&)> onComplete;
};
class ClipImplementation {
  virtual void stop()=0; virtual int play(float vol, std::function<void()> onEnd)=0; virtual void destroy()=0;
  virtual int playLoop(float vol, float freqRatio){return 0;}
  virtual void setChannelVolume(int ch,float){} virtual void setChannelFrequency(int ch,float){} virtual void stopChannel(int ch){}
};
class IAudioBackend {
  virtual std::unique_ptr<AbsMusic> createMusic(const std::string& path)=0;      // throws java::IOException
  virtual std::unique_ptr<ClipImplementation> createClip(const std::string& path)=0;  // never null (silent clip on failure)
  virtual std::unique_ptr<ClipImplementation> createStreamClip(const std::string& url)=0;
};
// registry: setAudioBackend(...) / audioBackend() -> SilentAudioBackend if none set
```
(Adjust only if Java requires it, and tell T2 in your notes.)
**Also port:** `MusicController` (including the muffle logic: `LOWPASS_OFF = 20000`, sweep interpolated in log-frequency, see `MUFFLE_NOTES.txt`), `SoundController`, `MultiClip`. `SilentAudioBackend` keeps a wall-clock song position so gameplay logic can be tested.
**Done when:** desktop tests run with the silent backend (position advances, pause/seek/pitch behave like Java).

## T2: BASS backend (app only)

**Owns:** `platform/audio/{BassBackend,BassMusic,BassClip,BassStreamClip}.{h,cpp}`, `third_party/bass/`.
**Needs:** T1's three headers, plus `bass.h` and `bass_fx.h` from the un4seen Android downloads (`c/` folder). Libs are already in `app/src/main/jniLibs/{arm64-v8a,armeabi-v7a}/`.
**BASS calls the Java version uses** (read from the compiled classes; the original `BassMusic.java` source would be better, ask the project owner):
- Init: `BASS_SetConfig` (sample rate), `BASS_Init`, `BASS_GetInfo`, `BASS_Free`, `BASS_ErrorGetCode`.
- Music: `BASS_StreamCreateFile` (decode) then `BASS_FX_TempoCreate` for pitch/tempo; `BASS_ChannelPlay/Pause/Stop`, `BASS_ChannelSetPosition/GetPosition`, `BASS_ChannelBytes2Seconds/Seconds2Bytes`, `BASS_ChannelSetAttribute`, `BASS_ChannelSetSync` (end event), `BASS_ChannelGetLevelEx`, `BASS_ChannelGetData` (FFT), `BASS_ChannelIsActive`, `BASS_ChannelGetInfo/Flags`, `BASS_StreamFree`. The Java code also has a `COMPENSATE_DEVICE_LATENCY` constant.
- Clips: `BASS_SampleLoad`, `BASS_SampleGetChannel`, `BASS_ChannelPlay`, `Get/SetAttribute`, `BASS_SampleStop`, `BASS_SampleFree`.
- Preview clips: `BASS_StreamCreateURL` + `BASS_ChannelSetSync`.
- Muffle (`setLowPass`): BASS_FX biquad low-pass, Q = 0.7071, channel all. At or above 20000 Hz remove the effect, otherwise create once and only update parameters afterwards (called every frame).
**Rules:** a failed load returns a silent clip, never null. Call `BASS_ChannelSetSync` callbacks on BASS's thread: marshal to the game thread, do not run game logic there.
**Done when:** it builds for both ABIs; a debug entry point plays `theme.mp3` from `assets/res` (extract or read via fd).

## T3: Logger (app only)

**Owns:** `platform/log/AndroidLogger.{h,cpp}`.
**Goal:** implement `opsu::ILogger` over `__android_log_print` (5 levels, exception overloads, tag passthrough). `setSaveToFile` writes to the opsu log folder.
**Done when:** `Log::setLogger(std::make_unique<AndroidLogger>())` shows lines in `adb logcat`.

## T4: Touch input (app only)

**Owns:** `platform/input/AndroidInput.{h,cpp}`.
**Reads (do not edit):** `core/include/opsu/input/{GameplayInput,TouchCursor,RawTouchListener,Keys}.h`, `Resolution.h`.
**Goal:** convert Android pointer events (down, move, up, cancel, per-finger ids) into calls on `GameplayInput` and `RawTouchListener`, with coordinates scaled to the game resolution. Per-finger rules are in the headers (a finger owned by the raw path stays owned until lifted). Provide a real `CursorSprite` hook point (the renderer fills it in later).
**Done when:** a fake-event test shows two fingers produce two independent cursors; cancel affects only that finger.

## T5: SQLite and the database layer

**Owns:** `core/include/opsu/db/*`, `core/src/db/*`, `platform/db/SqliteBeatmapDBStore.{h,cpp}`, `third_party/sqlite/`.
**Java reference:** `itdelatrisu/opsu/db/{BeatmapDB,ScoreDB,DBController}.java`. Existing: `BeatmapDBStore` interface and the delete/update slice of `BeatmapDB`.
**Goal:** (1) vendor the SQLite amalgamation. (2) Implement `BeatmapDBStore` over it. (3) Port the rest of `BeatmapDB` (schema/init, insert, load, last-modified map, clear, close), then `ScoreDB`. Keep the SQL and schema identical to Java so existing databases keep working.
**Done when:** a desktop test builds a db, inserts and loads a beatmap, and the existing batch37/38 tests still pass.

## T6: HTTP client (app only)

**Owns:** `platform/net/*`.
**Contract:** `core/include/opsu/downloads/HttpClient.h` (read the comments carefully): do **not** follow redirects, no caching, connect 5000 ms and read 10000 ms by default, `close()` on the body must be callable from another thread and make a blocked read fail with `java::IOException`, optional "skip certificate validation".
**Suggested route:** JNI to `HttpURLConnection` (no extra native dependency; the app needs a small Java activity anyway for the storage permission). libcurl for NDK is the alternative but needs a TLS library.
**Done when:** `Download` fetches a small file on a device or emulator.

## T7: Pure-logic ports (four independent agents, one file each)

Each agent owns **only** the files of its row. Port, golden-test against the real Java, notes.

| ID | Port | Java size | Needs |
|---|---|---|---|
| T7a | `BeatmapParser` | 31 KB | Options seam, file and MD5 seams, BeatmapDB seam |
| T7b | `StoryboardParser` | 14 KB | text-file reading seam; fills the existing `Storyboard` model |
| T7c | `AutoCursor` | 16 KB | `HitObject` and `Vec2f` (both ported) |
| T7d | `Slider` and `Spinner` logic halves | 39 KB and 23 KB | drawing stays out until T9 |

Also small clean-up (anyone, after T7 starts): replace the 3 private byte-trim copies with `java::trim` in `UserList.cpp`, `DownloadServer.cpp`, `BeatmapSearch.cpp`.

## T8: Delete hooks backends (after T1)

**Owns:** `core/include/opsu/beatmap/DeleteHooksBackend.h` and its `.cpp`, wiring only.
**Goal:** make the 6 missing `DeleteHooks` real: `trackActive`, `currentAudioFilename`, `resetTrack` (over T1's `MusicController`), and `watchServiceActive`, `pauseWatchService`, `resumeWatchService`. The 2 DB hooks come from T5.
**Done when:** the "do not call `deleteSong*`" rule can be removed.

## T9: Vulkan renderer (last, not parallel yet)

Not started. Do not split this across agents until one agent has frozen the renderer API (Vulkan 1.1 + VMA, `stb_image`, `stb_truetype` font atlas, sprite batch). After that, rendering halves can be split by file: `Curve` draw, `BeatmapSetNode` panels, `DownloadNode.drawResult`, `GameMod`/`PlaybackSpeed` buttons, `Cursor` (the real `CursorSprite`), `PjskStage`.

---

## Handoff template (put at the end of `docs/agents/<ID>.md`)

```
Task: <ID>   Status: done / partial
Done:        <files and what works>
WIP:         <what is not finished, and why>
Tests:       <how it was tested, and the commands>
Contract changes: <anything others must know>
```
