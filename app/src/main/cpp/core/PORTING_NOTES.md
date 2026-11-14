# opsu! libGDX -> C++17 port: progress

Build: `cmake -B build && cmake --build build && ctest --test-dir build`
(or: `g++ -std=c++17 -ffp-contract=off -Iinclude $(find src -name '*.cpp') tests/test_main.cpp`;
same for `tests/test_batch2.cpp`, `tests/test_batch5.cpp`, `tests/test_batch6.cpp`, `tests/test_batch7.cpp`, `tests/test_batch8.cpp`, `tests/test_batch9.cpp`, `tests/test_batch10.cpp`, `tests/test_batch11.cpp`, `tests/test_batch12.cpp`, `tests/test_batch13.cpp`, `tests/test_batch14.cpp`, `tests/test_batch15.cpp`, `tests/test_batch16.cpp`, `tests/test_batch17.cpp`, `tests/test_batch18.cpp`, `tests/test_batch20.cpp`, `tests/test_batch21.cpp`, `tests/test_batch22.cpp`, `tests/test_batch23.cpp`, `tests/test_batch24.cpp`, `tests/test_batch25.cpp`, `tests/test_batch26.cpp`, `tests/test_batch27.cpp`, `tests/test_batch28.cpp`, `tests/test_batch29.cpp`, `tests/test_batch30.cpp`, `tests/test_batch31.cpp`, `tests/test_batch32.cpp`, `tests/test_batch33.cpp`, `tests/test_batch34.cpp`, `tests/test_batch35.cpp`, `tests/test_batch36.cpp`, `tests/test_batch37.cpp`, `tests/test_batch38.cpp`, `tests/test_batch39.cpp`, and `tests/test_golden{,2,3,4,5,6,7,8,9,10,11,12}.cpp tests/golden/expected{,2,3,4,5,6,7,8,9,10,11,12}.txt` (golden10 also uses tests/golden/delete/; golden11 also reads tests/golden/cases11.txt, passed as its second argument))

> `-ffp-contract=off` matters: with FMA contraction (GCC default on ARM, or `-march=native` on x86)
> ~1000 golden lines drift in the last float bit. CMake sets it for you.

## Ported
### Batch 1
Vec2f, CurveType, Bezier2, CentripetalCatmullRom, LRUCache, LifeFrame, RawTouchListener,
OpsuConstants, logging/{ILogger,StubLogger,DefaultLogger}.

### Batch 2 (verified bit-for-bit against the real Java, see tests/golden/README.md)
| Java | C++ | Notes |
|---|---|---|
| (java.lang / java.text behaviour) | util/JavaCompat.h/.cpp | `String.split`, `parseInt/Float`, `(int)` cast, `%.Nf` (HALF_UP), `%g`, `Float.toString`, `DecimalFormat` |
| Utils (pure subset) | Utils.h | clamp x3, lerp, parseBoolean. Rest of Utils comes with its users |
| fake/Log | logging/Log.h/.cpp | static facade over ILogger; `setLogger(nullptr)` restores default |
| beatmap/TimingPoint | beatmap/TimingPoint.h/.cpp | |
| beatmap/HitObject | beatmap/HitObject.h/.cpp | `init(w, h, playfieldScale)` takes the scale until Options is ported |
| objects/curves/Curve, EqualDistanceMultiCurve, LinearBezier, CatmullCurve, CircumscribedCircle | same names | **geometry only** (see below) |
| replay/ReplayFrame | replay/ReplayFrame.h/.cpp | |
| replay/PlaybackSpeed | replay/PlaybackSpeed.h | enum class + `getModifier()`/`next()`; button part deferred |
| GameMod | GameMod.h/.cpp | logic half; `resetState()` = state part of `init()` |
| (libGDX Input.Keys letters) | input/Keys.h | A..Z with libGDX numbering; remap with the backend |

## Fixes to batch 1 found by the golden tests
* `Bezier2::pointAt`: Java's `c.x += x * b` (float += float*double) adds in double and narrows once;
  the C++ narrowed the product first -> 1-3 ULP drift in slider points. Fixed.
* `Vec2f::toString` / `LifeFrame::toString`: Java `%.Nf` rounds ties half-up (0.125 -> 0.13), printf
  rounds to even (0.12). Now use `java::formatFixed`.
* `CurveType::init`: `(int)` of a float is UB in C++ for NaN/huge; now `java::toInt`.

## Deliberate deviations / seams (all marked in the headers)
* **Rendering is not in `Curve` yet**: static `init(w,h,...)`, `borderColor`, `draw()`, `drawRange()`,
  `splice()`, `discardGeometry()` and the `SliderBodyRenderer` member are omitted until the graphics
  backend is chosen. They attach cleanly (Curve already exposes the points).
* `GameMod`: images, `MenuButton`s, category colour/position, `init()` layout, `draw/contains/hover*`
  deferred. `PlaybackSpeed`: same for its button.
* Java `null` arrays/strings are empty vectors/strings (`getSliderX()` is empty for circles,
  `getHitSoundFile()` is "" when absent). Output-equivalent; call sites should test `empty()`.
* Exceptions: malformed lines throw `java::NumberFormatException` (a `std::invalid_argument`) or
  `std::out_of_range`, where Java threw NumberFormatException / ArrayIndexOutOfBounds.
  `Curve` constructors can throw `std::runtime_error` (degenerate perfect curves, empty curve lists);
  Java callers already catch generically and fall back to `DummyObject`.
* `HitObject::init` no longer prints "Playfield Scale:..." to stdout.
* Curves hold a non-owning `const HitObject*`: the HitObject must outlive its Curve.

## Backend decision (made)
Android-native via the NDK, minSdk 29 (Android 10): windowing/touch = AGDK GameActivity (or
android_native_app_glue); graphics = native Vulkan 1.1 + VMA; images/fonts = stb_image / stb_truetype
(font atlases); audio = BASS (bass.h / libbass.so, already set up in the existing Android build).
Rule: everything under `include/opsu` + `src` that is game logic stays free of Android/Vulkan/BASS headers so
it builds and is golden-tested on desktop; backend code goes in a separate target that links `opsu_core`.
The audio seam from the muffle notes (setLowPass(cutoffHz)) maps onto a BASS_FX biquad low-pass.

## Intentionally not ported (replaced by the C++ standard library)
fluddokt.opsu.fake: Integer2, Long2, SlickException, FileReader/FileInputStream/FileOutputStream,
BufferUtils, UtilBuff, DefaultLogSystem, Effect/ColorEffect/GameState (empty), LWJGLException,
and the listener interfaces (LineListener, MusicListener, ...) which become std::function or small
interfaces when their users are ported.

## Intentionally not ported: dead download servers
Checked against the Java build (`states/DownloadsMenu.java` registers only `OsuDirectServer`, `CatboyServer`, `NekohaServer`, `HinamizawaServer`; `SongInfoHeader` uses OsuDirect + Catboy):
- **Abandoned services (project owner):** `RippleServer`, `MnetworkServer`, `HexideServer`, `BloodcatServer`.
- **Never in the build (commented out in `DownloadsMenu`, project owner confirms YaSOnline never appeared):** `YaSOnlineServer`, `MengSkyServer`, `OsuMirrorServer`.
None of the 7 is ported (RippleServer was ported in batch 24 and removed again). The Java files stay in the original core, `DownloadServer` is generic, and porting one later is just another subclass.

## Needs the backend (decided above) before porting
GL11/13/14/15/30/32, Graphics, Image, Font/UnicodeFont, Color, Display, Cursor, Keyboard, Input,
AppGameContainer, Clip/ClipImplementation/AudioBackend, Desktop, FileSystemLocation.
These wrap libGDX; in C++ they need a concrete choice (e.g. SDL2 + OpenGL + miniaudio/OpenAL).
They now block: GameObject, DummyObject (need Graphics), FakeCombinedCurve (pure rendering),
TouchCursor (needs ui/Cursor), and the UI halves above.

## Batch 3 (done): backend-free logic, verified against the real JVM
Ported: AnimationEquation, AnimatedValue, AnimationTrack, EasedSlide, LazerAnimations, KineticScrolling,
User, UserStats, Health, OsuReader, OsuWriter, plus the pure part of GameData (Grade, HIT_* constants,
getScorePercent, getGrade -> include/opsu/GameData.h) and Utils::{distance, standardDeviation,
mapDifficultyRange, getTimeString, bytesToString, cleanFileName}. New helpers: util/JavaMath.h
(Math.min/max/round semantics), util/JavaIO.h (IOException family, UTF-16 length/prefix,
JDK-exact lenient UTF-8 decode, compareToIgnoreCase).
Tests: tests/test_golden2.cpp vs tests/golden/expected2.txt (18k lines, regenerate with
tests/golden/java/gen2.sh <core/src>; Utils/GameData stubs are extracted from the real Java).

Deliberate API notes / known differences
- KineticScrolling: Java field `pressed` is `pressedFlag` so the public method stays `pressed()`.
- OsuWriter keeps Java quirks: float/double are written big-endian (reader is little-endian);
  strings write the UTF-16 length and the low byte of each UTF-16 unit (not UTF-8 for non-ASCII).
  writeULEB128 on negatives encodes 32-bit unsigned (Java would loop forever).
- User::setBio never splits a surrogate pair at the 90-unit cut (Java can leave a lone surrogate).
- compareToIgnoreCase is ASCII-only folding. Dates are int64 ms since epoch.
- UserStats::Play is a plain struct standing in for the DB ScoreData row (ScoreData itself needs the DB).
- AnimationTrack constants in LazerAnimations are `extern const` objects (defined in the .cpp).

## Changes made in the original Java core (from its *_NOTES.txt)
The verbatim notes are in docs/core-notes/. The core zip already contains all of these changes, so batch 3 was
ported from the changed code. What each one means for the C++ port:

| Core change (notes file) | Touches | C++ status |
|---|---|---|
| Profile v1/v2 (PROFILE_V1/V2) | User (+banner/bannerFile/avatarFile/bio, 90-char bio), UserStats (new), UserStats from ScoreDB, UserButton, UserProfileOverlay, ProfileImages, ScoreDB (users table +4 columns, DB version 20261106) | **User, UserStats ported** (incl. new fields). UserStats::Play stands in for the ScoreDB row. Still blocked: UserButton, UserProfileOverlay, ProfileImages (backend), ScoreDB migration + getUserStats (DB). |
| Downloads UI v1-v4, tutorial, servers (DOWNLOADS_*, DOWNLOADER_PORT) | DownloadNode, DownloadsMenu, DownloadsOverlay, DownloadThumbnails, DownloadPreview, BeatmapInfoPanel, DownloadsTutorial, DropdownMenu, AAShapes, Catboy/OsuDirect/Nekoha/Hinamizawa servers | Not ported (UI + network). **EasedSlide ported** (added by v3). Download servers (JSON/HTTP parsing) are backend-free but need an HTTP client choice (libcurl?). BeatmapOnlineInfo was dropped in the core too. Tutorial/overlays only use OUT_QUART/OUT_CUBIC/IN_OUT_SINE, all in AnimationEquation. |
| Song menu header v5-v10 (SONGMENU_HEADER) | SongInfoHeader, SongMenu, Colors (+getDownloaderStarColor, getDividerColor, HEADER_* colours), DownloadServer.lookupSet | Not ported (UI). Divider width constant DIVIDER_LINE_WIDTH = 2; divider colour = skin MenuGlow else default blue. |
| Cursor effects (CURSOR_EFFECTS) | Cursor trail, CursorParticles, GameObject/Slider/Spinner/Game hooks | Not ported (rendering). Rules to keep: trail with cursormiddle = smooth additive 500 ms, without = disjoint 150 ms @60 Hz; particles owned by Game: break 20/s, kiai 60/s. |
| Muffled music (MUFFLE) | MusicController.setMuffled/updateMuffle/onVolumeAdjusted, AbsMusic.setLowPass(hz), Options volume setters | Not ported (audio). **Backend requirement**: the audio backend must implement setLowPass(cutoffHz): >= 20000 (LOWPASS_OFF) removes the filter, otherwise one biquad low-pass (Q 0.7071) whose parameters are updated every frame; sweep is log-frequency, 300 Hz muffled, ~0.5 s in / ~0.7 s out, volume adjust lifts it and eases back after 1.5 s. Add this to the miniaudio/OpenAL choice. |
| Intro with real beatmap (INTRO_REAL_BEATMAP) | RhythmIntro, IntroMusic (parses hit objects, picks first kiai/downbeat with >= 3 objects), Splash, DynamicFreeTypeFont/Font destroy fix | Not ported. Needs Beatmap/BeatmapParser (next batch) and rendering. The DynamicFreeTypeFont fix is libGDX-specific (disposed glyph page), irrelevant to a new font backend. |
| Lazer animations | LazerAnimations, AnimationTrack | **Ported and golden-tested** (batch 3). |

Behaviour notes worth keeping in mind when the rest is ported
- ScoreDB.getUserStats() was moved off the GL thread (shared connection; wrap in a lock if the SQLite driver is not thread-safe).
- ProfileImages API changed to ProfileImages.Ref + KIND_* constants; images are processed on a background thread, only texture upload is on the GL thread.
- Taps in the downloads UI fire on release; UserProfileOverlay ignores a release whose press started before it opened (OPEN_TAP_GUARD_MS).
- Old-core pieces intentionally not in the current core: BeatmapOnlineInfo, SongInfoPanel, slideshow images.

## Batch 4 (done): Beatmap + HP drop rate, verified against the real JVM
Ported: Beatmap (data, ordering, text, DB string codecs, ModParams / mod-star cache, play stats),
BeatmapHPDropRateCalculator, util/Color.h (fake.Color semantics), and `java::floatToIntBits` / `java::compare(float,float)`
(Float.compare) in util/JavaMath.h.
Tests: tests/test_golden3.cpp vs tests/golden/expected3.txt (3.4k lines, regenerate with
tests/golden/java/gen3.sh <core/src>). The real Beatmap.java, BeatmapHPDropRateCalculator.java, Health, HitObject,
TimingPoint, GameMod, curves and fake.Color/Integer2/LRUCache are compiled unmodified; only the engine edges are stubbed
(tests/golden/java/stubs3). The golden driver was mutation-tested: 19 deliberate C++ bugs, all caught except one that is
behaviourally equivalent (computing `recovery` in double: `uncapped - 200` is exact in float).

Seams / deviations
- Options.* is passed explicitly: `getTitle/getArtist/toString(useUnicodeMetadata)`, `getComboColors(skinColors)`,
  `getSliderBorderColor(skinBorder)`, `ModParams::current(fixedSpeed, fixedCS)` (reads GameMod itself), `getStarRating(current)`,
  `setModStars(p, stars, current)` (Java re-read the globals inside each call).
- Nullable Java fields are `std::optional` (null != empty matters: `timingPoints == null` means "not loaded from the DB"):
  breaks, timingPoints, combo, sliderBorder, objects. File fields are `std::string` paths with "" = null.
- BeatmapHPDropRateCalculator does NOT load anything: the Java ctor called BeatmapDB.load + BeatmapParser.parseHitObjects.
  Callers must pass a beatmap with `timingPoints` and `objects` set; `calculate()` throws (logic_error / out_of_range)
  in the same order the Java would NPE / AIOOBE.
- Not ported (rendering): the background-image LRU cache, loadBackground / isBackgroundLoading / hasLoadedBackground /
  drawBackground / clearBackgroundImageCache.
- Java quirks kept: `compareTo` uses Float.compare then Integer2.compare (`i - j`, wraps on overflow); a failed
  `breaksFromString` leaves the partially filled list; `timingPointsFromString(null)` yields an empty (non-null) list;
  `Color.getRed()` truncates ((int)(r*255)) - exact for int channels 0..255, truncating for arbitrary float colours.
- Findings worth knowing (HP calculator): the "3 combo ends too low" branch is unreachable for circle-only maps (a
  combo-ending circle gains 20 HP = the gap between the two thresholds at drain 10); it needs sliders/spinners, whose duration
  drain is applied after the "ever" check. The golden maps include slider-only runs so that branch is covered (93 hits).

## Batch 5 (done, small): SoundEffect, MD5InputStreamWrapper
Ported (smallest unported pure-logic files first): SoundEffect, io/MD5InputStreamWrapper, plus util/MD5.h/.cpp
(RFC 1321, replaces `java.security.MessageDigest("MD5")`; the C++ standard library has no digest).
Tests: tests/test_batch5.cpp (MD5 vs Python hashlib at padding boundaries 0/55/56/63/64/65/119/120/128 B ... 100 KB, fed in
1/7/63/64/65-byte chunks; wrapper partial reads, EOF, cached getMD5; enum names/order checked against the Java source).
No golden JVM run for this batch (nothing float/locale dependent).

Seams / deviations
- SoundEffect: `enum class SoundEffect` in declaration order (= Java `ordinal()`), `getFileName(e)`, `soundEffectValues()`,
  `SoundEffect_SIZE` (19). The Java `MultiClip clip` field (getClip/setClip) is NOT stored in the enum: clips are audio-backend
  objects, so SoundController will keep a `std::array<Clip, SoundEffect_SIZE>` indexed by the enum.
- MD5InputStreamWrapper wraps a non-owned `std::istream&`. `read()` -> 0..255 / -1; `read(buf, len)` -> count / 0 for len 0 / -1 at EOF;
  `skip()`/`reset()` throw `std::runtime_error` (Java RuntimeException); `markSupported()` is always false.
  The Java ctor's NoSuchAlgorithmException cannot occur. `mark()` is dropped (a std::istream has no mark/reset contract).
- `MD5::digest()` resets the state afterwards, like `MessageDigest.digest()`.

Looked at and deliberately skipped this round
- KiaiFlash (2 KB): pure drawing (additive-blend sprite), waits for the backend.
- DownloadImporter (1.3 KB): thin glue over Options.getDownloadDir/getSkinRootDir/getBeatmapDir + SkinUnpacker/OszUnpacker
  (needs Options and the unpackers first).
- TouchCursor, DummyObject, FakeCombinedCurve, StarFountain: need ui/Cursor / Graphics.

## Batch 6 (done, small): BeatmapSortOrder
Ported: beatmap/BeatmapSortOrder (enum + the 7 comparators + saved/current/set) and `java::integer2Compare` / `java::long2Compare`
in util/JavaMath.h (fake.Integer2/Long2: `i - j` and `(int)(i - j)` with Java wrap-around, so results are NOT -1/0/1).
Tests: tests/test_batch6.cpp (behavioural, no JVM golden: the box has a JRE but no javac; the logic is plain int/long arithmetic).

Seams / deviations
- BeatmapSetNode / BeatmapSet are not ported (UI/state), so comparators take the set's beatmaps (`std::vector<Beatmap>`,
  alias `BeatmapSetBeatmaps`): `compare(order, a.getBeatmapSet(), b.getBeatmapSet())`, `less(...)` for std::stable_sort
  (Collections.sort is stable). When BeatmapSetNode is ported its `getComparator()` should just forward to these.
- `Options.getSavedSortOrder()` is injected: `setSavedSortOrderProvider(fn)` returning the enum name or nullopt.
  `saved()` / `current()` / `set()` keep the Java lazy-load rules (current() loads once; set() before current() suppresses the load).
  Name lookup is case-sensitive like `sort.name().equals(name)`; unknown/absent -> TITLE. `resetForTesting()` has no Java counterpart.
- Java comparator getters `getName()` / `name()` are free functions `getName(order)` / `name(order)`.
- TITLE/ARTIST/CREATOR/BPM use `.at(0)` (Java `get(0)` -> IndexOutOfBounds) so an empty set throws std::out_of_range.
  LENGTH/DATE/PLAYS just give 0 for an empty set, as in Java. PLAYS sums in uint32 (defined wrap, same bits as Java int).
- LENGTH keeps the Java start value 0 (an unset endTime of -1 never wins); DATE likewise with dateAdded.

## Batch 7 (done, small): PJSK mode foundations (fourresonance/)
Picked the smallest unported *pure-logic* files: PjskJudge (4 KB), PjskChart (5 KB), PjskNote (3 KB), plus the two BPM helpers
from OsuToPjsk that PjskChart calls. (The Java package is spelled `fourresonace` in the sources; the directory and the C++
namespace are `opsu::fourresonance`.)
Ported: fourresonance/{PjskJudge.h, PjskChart.h/.cpp, PjskNote.h/.cpp, OsuToPjsk.h/.cpp (PARTIAL here; the converter was finished in batch 39)}.
Tests: tests/test_batch7.cpp (behavioural, no JVM golden: no javac on the box; expected numbers computed from the Java formulas).

Seams / deviations
- PjskJudge is header-only: `enum class Judgment` + `multiplier(j)` / `name(j)` (Java enum field), `Window` is a constexpr struct,
  and TAP_NORMAL / TAP_CRITICAL / SLIDE_END / SLIDE_END_TRACE are `inline constexpr` (no static-init-order problems).
  The Java `frames(perfect, null, null)` is the one-argument `detail::frames(perfect)`.
- PjskNote keeps `const PjskChart::Note* data` (non-owning, like Curve -> HitObject): the chart's `notes` vector must outlive it
  and must not reallocate. `headJudgment` / `tailJudgment` are `std::optional<Judgment>` (null = empty). Fields are plain (not const)
  so notes can live in a `std::vector<PjskNote>`. `headWindow()` / `tailWindow()` return const references to the constants.
- PjskNote lane edges: Java computes `data.lane - data.size` in float and widens; the C++ does the same.
- PjskChart::scaledTime is `const` with a `mutable` cache. Java quirk kept: the cache is built once and never rebuilt, so adding
  timeScale entries afterwards makes lookups past the old end throw `std::out_of_range` (Java: ArrayIndexOutOfBounds).
- OsuToPjsk is only `timeMsToBeat` / `beatToTimeMs` (1e-6 ms tolerance at segment starts, empty list = 500 ms/beat). The converter
  (24 KB) needs mania HitObject parsing and comes later. *(Done in batch 39.)*
- Not ported yet in this package: PjskStage (9 KB, rendering),
  PjskGame (18 KB, state/UI), OsuToPjsk converter (24 KB, *done in batch 39*).

Also fixed in CMakeLists.txt: `add_test(opsu_batch5 ...)` had a stray `test_batch6` argument and batch 6 had no test registered;
now opsu_batch5, opsu_batch6, opsu_batch7 are separate tests.

## Batch 8 (done, small): PjskGameplay
Ported: fourresonance/PjskGameplay.h/.cpp (touch-driven judging, holds, tails, combo, score) and fourresonance/PjskStage.h (PARTIAL:
only the constants HITBOX_LENIENCY = 1.0 and HITBOX_VERTICAL_HALF_LANES = 5.0; drawing and touchToLane() wait for the backend).
Tests: tests/test_batch8.cpp (behavioural scenarios derived by hand from the Java; no JVM golden, no javac on the box):
hitbox edges, good-window edges, miss boundary, overlapping-note choice (closer lane, older note), hold release / slide-off /
never released, missed hold = 2 misses, TRACE and NONE tails, critical window, score rounding.

Seams / deviations
- `Listener` is `std::function<void(const PjskNote&, bool tail, Judgment, double diffMs)>`; empty = Java null.
- `notes` is a `std::vector<PjskNote>` (never resized after construction) and `held` is a `std::vector<PjskNote*>`, so the class is
  non-copyable. Each PjskNote points at the chart's Note: the chart (and its `notes` vector) must outlive the gameplay.
- `counts` is `std::array<int,4>` indexed by `static_cast<size_t>(Judgment)`; `lastJudgment` is `std::optional<Judgment>`.
- `finishTail(n, null, ...)` takes `std::optional<Judgment>` (nullopt = not judged). `held.remove(n)` is identity removal.
- `score()` uses `java::round` (floor(x + 0.5)), same as Math.round.

## Batch 9 (done, tiny): PjskAuto
Ported: fourresonance/PjskAuto.h/.cpp and `java::compare(double,double)` (Double.compare: -0.0 < 0.0, NaN greatest) in util/JavaMath.h.
Tests: tests/test_batch9.cpp (full autoplay of a chart with tap/hold/trace/none notes gives all PERFECT and score 1,000,000;
touches arrive at their exact event time, not the frame time; zero-length hold ordering; empty chart; Double.compare cases).

Seams / deviations
- Takes `PjskGameplay&` (non-owning, must outlive the auto player). Java's `Arrays.sort(Integer[], Comparator)` becomes `std::sort`
  over indices: the comparator is a strict total order (time, then down-before-up, then index), so stability does not matter.
- Parallel arrays are vectors (`down` is `std::vector<bool>`).
- The PJSK package now has everything except: PjskStage (rendering), PjskGame (state/UI), and the OsuToPjsk converter *(converter done in batch 39)*.

## Batch 10 (done, tiny): Resolution, ReadableByteChannelWrapper, UselessUtils (partial)
Picked the smallest unported files that are not blocked on the backend (everything smaller is in the "intentionally not ported"
list or is a backend wrapper). Ported: Resolution.h (264 B), downloads/ReadableByteChannelWrapper.h (1.7 KB) with the small
io/ReadableByteChannel.h interface it needs, and the backend-free part of kww/UselessUtils (3 KB): kww/UselessUtils.h/.cpp.
Tests: tests/test_batch10.cpp (behavioural, no JVM golden: no float/locale logic). Mutation-checked: 5 deliberate bugs (count -1,
32-bit counter, alpha loop from 0, month off by one, 12-hour clock), all caught.

Seams / deviations
- Resolution is `opsu::Resolution` (class, getWidth/getHeight). It is `fluddokt.opsu.fake.Resolution`, NOT the private
  `Options.Resolution` enum (list of selectable window sizes), which ports with Options.
- `ReadableByteChannel` is a small abstract class standing in for java.nio's: `int read(uint8_t* buf, size_t len)` (count, 0, or
  -1 at EOF; replaces `read(ByteBuffer)`), `isOpen()`, `close()`. Implementations may throw `java::IOException`. Download (HTTP
  client still undecided, libcurl?) will implement it.
- ReadableByteChannelWrapper holds a non-owning reference (the channel must outlive it). Java quirk kept: only results > 0 are
  counted, so 0 and the -1 EOF marker add nothing; a read that throws counts nothing. `getReadSoFar()` is `long long`. Like the Java
  field it is not synchronised: Download reads it from the UI thread while the download thread writes it, so make it
  `std::atomic<long long>` (or lock) when Download is ported.
- UselessUtils is PARTIAL. Ported: `screenshotFileName(std::tm)` / `(time_t)` (`screenshot_yyyyMMdd_HHmmss.png`, 24-hour, local
  time zone) and `makeOpaque(rgba, sizeBytes)` (alpha byte of every complete RGBA pixel -> 255). Not ported (backend):
  `takeScreenshot()` (frame-buffer read-back, thread, SHUTTER sound, notification, Options.getScreenshotDir()) and
  `savePixmapTo()` (PNG encode + the vertical `flip`, which belongs in the PNG writer; GL read-back is bottom-up).
  The Java opaque loop is bounded by backbuffer W*H*4, here by the size you pass: pass width*height*4.

Looked at and deliberately skipped this round
- DeviceInfo (267 B): Android hook (`getInfo()` = "", `getDownloadDir()` = null, `setFPS()` no-op). Becomes a backend/platform
  interface with the Android target; nothing to port as logic.
- PulseAudioFixerListener (2 KB): works around the OpenJDK icedtea-sound PulseAudio clip bug. javax.sound is not used in the C++/BASS
  audio backend, so there is nothing to fix.
- DBController (2 KB): JDBC driver loading + `jdbc:sqlite:` connection. Waits for the SQLite choice (sqlite3 C API) together with
  BeatmapDB / ScoreDB; `init()` / `closeConnections()` / `createConnection()` map 1:1 then.
- MultiClip (267 B): empty subclass of Clip (audio-backend object, see SoundEffect notes).

Build fix: `add_test(opsu_batch5 ...)` in CMakeLists.txt still had the stray `test_batch6 test_batch7 test_batch8 test_batch9`
arguments and batches 6-9 had no test registered (the batch 7 note said this was fixed, but the file in this archive had the
old line). Now opsu_batch5 .. opsu_batch10 are separate tests, and UselessUtils.cpp is in the `opsu_core` sources.
Verified with the plain g++ line (no cmake on the box): all test executables and golden 1/2/3 pass (golden 6740 / 18194 / 3417 lines).

## Batch 11 (done, tiny): UserList
Ported: user/UserList.h/.cpp (4.4 KB Java) plus the `UserStore` interface it needs. Picked as the smallest unported file with real logic
that is not blocked on the backend: DownloadImporter (1.3 KB) is only glue over Options + SkinUnpacker/OszUnpacker, so it waits for those.
Tests: tests/test_batch11.cpp (behavioural, no JVM golden: no float/locale logic). Covers empty store -> Guest, restoring the saved
user (case-insensitive), missing/null saved user -> Guest (created or reused), name validation (empty, 16/17 units, trim, UTF-16
surrogate pairs, existing, "opsu!" Auto name), create/change/delete, singleton overwrite. Mutation-checked: 3 deliberate bugs (length
off by one, trim removed, case-sensitive delete check), all caught.

Seams / deviations
- `ScoreDB` is not ported (needs SQLite), so UserList takes a `UserStore&` (getUsers / getCurrentUser / setCurrentUser / updateUser /
  deleteUser = exactly the 5 ScoreDB calls the Java made). The future ScoreDB implements it. The store must outlive the list.
  `getCurrentUser()` returns `std::optional<std::string>` (nullopt = Java null).
- `UserList::create(store)` / `get()` keep the Java singleton (`get()` is nullptr before `create()`).
- Users live in `std::map<std::string, User>` keyed by lower-cased name (ASCII folding; Java used Unicode `toLowerCase()`).
  `getUser()` returns `User*` (nullptr = null) and `currentUser` is a pointer into that map: valid until that user is deleted.
  `getUsers()` returns sorted copies (stable sort by `User::compareTo`).
- **Deliberate deviation**: Java `deleteUser` compared `name.equals(currentUser.getName())` (case-sensitive), so `deleteUser("guest")`
  while "Guest" was current deleted the current user from the map. In C++ that would leave a dangling pointer, so the check is
  case-insensitive (matches the map key). Only differs for calls with a different-case spelling of the current user's name.
- `userExists(null)` has no C++ equivalent (std::string cannot be null); pass "" (false either way).
- `isValidUserName` uses UTF-16 length (<= 16) and Java `trim()` semantics (chars <= U+0020); Auto-name check is ASCII case-insensitive.
- CMakeLists.txt: UserList.cpp added to `opsu_core`, `opsu_batch11` test registered.

## Batch 12 (done, tiny): DownloadImporter
Ported: beatmap/DownloadImporter.h/.cpp (1.3 KB Java) plus the `DownloadImporterEnv` seam. Tests: tests/test_batch12.cpp (behavioural, no JVM
golden): unset / missing / not-a-directory folder does nothing; both unpackers get (downloadDir, own destination); skins run on another
thread, beatmaps on the caller; importAll joins (slow skin thread is finished on return); a throwing beatmap or skin unpacker (incl. a
non-std exception) never escapes and never stops the other one. Mutation-checked: 3 deliberate bugs (no join, no isDirectory check, wrong
skin destination), all caught.

Seams / deviations
- Options and the unpackers are not ported, so `DownloadImporterEnv` supplies `getDownloadDir/getSkinRootDir/getBeatmapDir` ("" = null)
  and `unpackSkins/unpackBeatmaps(src, dest)`. `importAll(env)` takes the env instead of reading statics. When Options, OszUnpacker and
  SkinUnpacker are ported, one small adapter implements the env (the Java `File[]` return values were ignored here, so they are `void`).
- Java: `Thread` "opsu-download-skins" with `setDaemon(true)`. C++: `std::thread`, always joined before returning, so daemon-ness is moot.
  Like Java, an exception on the skin thread is caught there (an uncaught one in a std::thread would call std::terminate).
- Java `catch (Exception)` + `printStackTrace()` -> `catch (std::exception)` / `catch (...)` + `Log::error`.
- The Java `InterruptedException` around `join()` has no C++ equivalent (join cannot be interrupted).
- `isDirectory` uses std::filesystem on a UTF-8 path.
- CMakeLists.txt: DownloadImporter.cpp added to `opsu_core`, `opsu_core` now links `Threads::Threads`, `opsu_batch12` test registered
  (plain g++ builds need `-pthread`).

## Batch 13 (done, tiny): HitSound
Ported: audio/HitSound.h/.cpp (4.7 KB Java): `enum class HitSound` (7), `enum class SampleSet` (3) and the static sample-set state.
Tests: tests/test_batch13.cpp (behavioural, no JVM golden: plain enum/string logic). Covers enum order and file/set names, default by
name (case-insensitive, "auto"/unknown/untrimmed -> NORMAL), default by index (1..3, 0/4/negatives -> NORMAL), `resolveSampleSet`
(explicit wins, 0/invalid -> default, no state change) and the deprecated `setSampleSet` (starts from the default, later default
changes do not move it). Mutation-checked: 3 deliberate bugs (setSampleSet ignoring the default, resolve comparing the enum value
instead of the index, case-sensitive name match), all caught.

Seams / deviations
- Same seam as SoundEffect: the Java `HashMap<SampleSet, MultiClip> clips` (getClip/setClip) is not stored here. The SoundController
  keeps the clips, indexed by (HitSound, SampleSet). Java `getClip()` with no argument reads the current sample set:
  `hitsound::activeSampleSet()` returns it (nullopt = Java null, i.e. before the first `setSampleSet`).
- `HitSound.getFileName()` / `SampleSet.getName()` / `getIndex()` are free functions (`getFileName(h)`, `getName(s)`, `getIndex(s)`);
  `SampleSet` is a namespace-level enum, `SampleSet_SIZE` = 3 and `HitSound_SIZE` = 7. The index is `ordinal() + 1` (NORMAL 1, SOFT 2, DRUM 3).
- The static methods live in `namespace opsu::hitsound`. Java overloads `setDefaultSampleSet(String)` / `(byte)` become
  `setDefaultSampleSet(const std::string&)` / `setDefaultSampleSetType(int8_t)`: with both overloaded, a literal `0` would be ambiguous
  with a null pointer. Java `byte` = `int8_t`, so values outside -128..127 do not exist (same as Java).
- The state is plain statics, like the Java fields: not thread-safe (the Java `setSampleSet` is `@Deprecated` for exactly that
  reason; prefer `resolveSampleSet` + a per-set clip lookup). `resetForTesting()` has no Java counterpart.
- Name matching is ASCII case-insensitive (Java `equalsIgnoreCase`).

## Batch 14 (done, small): PerformanceCalculator
Ported: beatmap/PerformanceCalculator.h/.cpp (6.9 KB Java): the aim / speed / accuracy / reading pp estimate and its `Result`
(played score + perfect-play maximum, `ratio()` helpers). Pure math on top of the aim/speed star values.
Tests: tests/test_batch14.cpp (no JVM golden: no javac on the box). Expected numbers come from an independent Python re-implementation of
the Java formulas (float32 where Java uses float, wrapped int32 where Java ints wrap), 12 scenarios covering HR/EZ/HD/FL/NF/SO, AR bonus
on both sides of the thresholds, HR clamp, unknown AR / max combo / object count, long maps (> 2000 hits), 0 stars and more hits than
objects; plus the early outs, the throwing provider, int overflow of the hit total and `ratio()` clamping. Compared with a relative
1e-9 tolerance (C++ `std::pow`/`log10` vs Java's `Math.pow`/`log10` may differ in the last ulp; not bit-exact like the golden batches).
Mutation-checked: 15 deliberate bugs (formula constants, thresholds, clamp, missing wrap...), all caught except two behaviourally
equivalent ones (`maxCombo <= 0` vs `< 0`, and `totalHits > 2000` vs `>= 2000` where log10(1) = 0).

Seams / deviations
- (Resolved in batch 15: `BeatmapDifficultyCalculator` is ported and `calculate(beatmap, ...)` exists.) `BeatmapDifficultyCalculator` (22 KB) was not ported yet. The Java `calculate()` built it internally; here `calculate(provider, beatmap, ...)`
  takes a `StarsProvider` (`void(const Beatmap&, double& aimStars, double& speedStars)`, may throw) and `calculateFromStars(aimStars,
  speedStars, beatmap, ...)` takes the star values directly. When the calculator is ported the provider is one lambda (shown in the header).
  A throwing provider logs "Failed to compute difficulty for the performance breakdown." and returns all zeroes, like the Java catch.
- Namespace `opsu::PerformanceCalculator` with `struct Result` (Java: static class, private constructor).
- Java float details kept: `od`/`ar` are float (HR `* 1.4f` clamped with `Math.min(10f, ...)`, EZ `* 0.5f`), `(od * od)` is a float product
  widened afterwards, `Result.ratio` returns float. `Math.min/max` are `java::min/max` (NaN wins). Java int overflow in the hit total and
  `h300 - (totalHits - objectCount)` wraps (done with unsigned arithmetic). `objects == null` is `std::optional` empty.
- The one-sided case `maxCombo <= 0` falls back to `combo`, as in Java.

## Batch 15 (done): BeatmapDifficultyCalculator + golden4 + a segfault fix
Ported: beatmap/BeatmapDifficultyCalculator.h/.cpp (22 KB Java; osu!tp strain algorithm: speed/aim difficulty and stars) and
`PerformanceCalculator::calculate(beatmap, ...)`, the Java-signature overload that builds the calculator itself (the `StarsProvider`
overload and `calculateFromStars` are unchanged).

**Golden suite 4 (`tests/test_golden4.cpp`, `tests/golden/expected4.txt`, `java/Golden4.java`, `java/gen4.sh`)** runs the REAL
`BeatmapDifficultyCalculator` and `PerformanceCalculator` (+ real `Beatmap`, `HitObject`, curves; DB/parser edges stubbed in `stubs3`):
360 random maps (mixed / stream / slider-heavy; every slider type; random timing points; 6 resolutions and playfield scales; odd
`circleSize`/`clockRate` values incl. a zero circle radius and NaN), the pre-`HitObject.init` state, unloaded/empty/degenerate beatmaps,
timing-point boundary cases, and 160 random scores through `PerformanceCalculator` (all mods, unknown max combo, zero hits, int overflow).
This also verifies the batch-14 `PerformanceCalculator` against the real Java (batch 14 used a Python model; `javac` IS usable here via
`java -m jdk.compiler/com.sun.tools.javac.Main`, which is what `gen*.sh` do).

*`pow` tolerance.* Everything that is pure arithmetic is compared bit-for-bit. Double results downstream of `Math.pow` may differ by up to
16 ulp (`--strict` demands bit-exactness): over 40,000 calls with these calculators' argument ranges, HotSpot's `Math.pow` is correctly
rounded 99.94% of the time, glibc `pow` 99.93%, fdlibm/`StrictMath` only 90.1%; HotSpot and glibc disagree on 0.09% of calls and neither
is systematically right (so no C++ code can be "bit-exact with Java" here: Java itself depends on the JVM, and Android's ART/bionic is a
different `pow` again). Observed: 733 of 734 lines bit-exact vs HotSpot, the other within 2 ulp; against Java run with fdlibm
(`-XX:+UnlockDiagnosticVMOptions -XX:-UseLibmIntrinsic`) 162 lines drift, max 9 ulp, all within tolerance. A real narrowing bug is ~1e8 ulp.

**Fix (batch 2 code): negative `pixelLength` crashed.** `EqualDistanceMultiCurve` (and `CircumscribedCircle`) sized the point array with
`size_t(ncurve) + 1`; for a slider with a small negative length (`ncurve == -1`) that wraps to an EMPTY vector and `curve_[0]` was a null
dereference (SEGV, confirmed with ASan), where Java throws (`ArrayIndexOutOfBounds` / `NegativeArraySizeException`) and callers fall back.
A malformed `.osu` could have crashed the game. Now throws `java::NegativeArraySizeException` / `std::out_of_range` like Java.
Regression tests in test_batch15.cpp (fail with the old files, pass with the fix). Found by running the calculator on negative lengths.

Seams / deviations
- Like `BeatmapHPDropRateCalculator`, the constructor does not load anything (Java: `BeatmapDB.load` + `BeatmapParser.parseHitObjects`):
  pass a beatmap whose `timingPoints` and `objects` are loaded, else `calculate()` logs the Java error and leaves everything at -1.
- The log message uses `beatmap.toString(false)` (Java reads `Options.useUnicodeMetadata`, not ported). The beatmap is referenced, not
  copied. Constructors/destructor are out of line (the strain helper `TpHitObject` is private and incomplete in the header).
- Java `(int)` casts use `java::toInt`, `Collections.sort(.., reverseOrder())` uses `Double.compare` ordering (`java::compare(double,double)`),
  and the `(time - startTime) / sliderTime` int division is kept.

**Findings: Java behaviours kept bit-for-bit that look unintended** (pinned by characterization tests in test_batch15.cpp; if you fix the
Java, edit it, run `gen4.sh`, and those tests show what moved). I have not compared with the original osu!tp C#.
1. `tpSlider.getPositionAtTime` does `float t = (time - startTime) / sliderTime;` with two ints, so `t` is always an integer. The first
   segment therefore always evaluates the curve at 0 (`lazySliderLengthFirst` is always 0 and, for odd repeat counts, the slider "ends" at
   its start) and later segments always at 1: slider shape never matters, only the start-to-end distance. A float division would fix it.
2. The strain of the last (partial) 400 ms interval is never added to `highestStrains`, so objects that all fall inside the final interval
   contribute nothing (a map whose objects all lie within the first 400 ms has difficulty 0).
3. The slider curve is built unscaled (`getSliderCurve(false)`) but `EqualDistanceMultiCurve` multiplies `pixelLength` by
   `HitObject.getXMultiplier()`, so star ratings depend on the screen resolution / last `HitObject.init()` (and on a still-zero
   multiplier before it).
Mutation-checked: 29 deliberate bugs in the calculators and the curve guard (strain/decay/step constants, sort order, repeat parity,
circle radius, follow-circle radius, clock-rate guard and its use in the interval test, timing-point `>`/`>=`, inherited-multiplier base,
`Math.pow` exponents, HR/EZ factors, `starsToPp`, miss penalty, ratio clamp, HD bonus, float-vs-double `od * od`, the Java int-division and
last-interval quirks, the negative-length guard): 27 caught (golden4 and/or batch15), 2 behaviourally equivalent (the `isSpinner()` branch:
a spinner is never `isCircle()` either; the `objectCount` fallback: unreachable, the calculator has already failed). A first run missed
the timing-point boundary (random data never puts a timing point exactly on an object's time), which is why the DIFFTP cases exist.

## Batch 16 (done, tiny): TouchCursor + GameplayInput
Ported: input/TouchCursor.h (1.3 KB Java) and input/GameplayInput.h/.cpp (5.9 KB Java): the multi-finger gameplay tracker (per-pointer cursors,
finger limit, "main" finger = most recently pressed one still down). Picked as the smallest unported pure-logic pair (everything smaller is
a backend wrapper or already on the "not ported" list; DownloadList needs Download + DownloadNode, 14 KB + more, so it waits).
Tests: tests/test_batch16.cpp (behavioural, no JVM golden: integer state machine, one float distance check). Covers pool/defaults, limit
clamp, press/move/release, duplicate down, finger limit + rejected finger flags, ignore(), main-finger fallback on release, oldest finger,
position memory (incl. after reset), keyBit lifetime, reset(), findDownWithin (nearest, tie, strict radius, float), and the exact sprite
call sequence (reset on press/release, update/draw only for fingers that are down, reset of all 20 on reset()).
Mutation-checked: 26 deliberate bugs, all caught except one behaviourally equivalent (`pressOrder >= bestOrder`: press orders are unique).
The first round missed one real gap (position memory is only visible after reset()); a test was added for it.
Full suite (cmake, 18 tests incl. golden 1-4) passes.

Seams / deviations
- The per-finger `ui.Cursor` is rendering, so it is a seam: `CursorSprite` interface (resetLocations / update(delta) / draw(x, y, pressed) are
  the only 3 calls TouchCursor/GameplayInput make on it) + `NullCursorSprite` default. `GameplayInput(SpriteFactory)` creates one sprite per
  pointer (factory may be empty or return null -> no-op sprite). The backend's Cursor implements `CursorSprite`.
- Java returned `TouchCursor` / `null`, here `TouchCursor*` / `nullptr`. The pool is a `std::vector<TouchCursor>` of 20, reserved up front and
  never resized, so pointers stay valid; like Java the object is reused, so do not keep it across a release. `getCursors()` returns
  `const std::vector<TouchCursor>&` (Java: the array). GameplayInput is not copyable (owns the sprites).
- Java package-private TouchCursor fields (down, ignored, x/y, downX/downY, pressOrder, sprite) are private with `GameplayInput` as friend.
  `id` and `keyBit` stay public like Java. Added `isIgnored()` (no Java getter; the field was package-visible and the tests need it).
- `pressOrder` / `pressCounter` are `int64_t` (Java `long`). `findDownWithin` keeps the Java float arithmetic (`int - float` is float,
  `d2 < best` strict, so a finger exactly on the circle is not inside and a tie keeps the lowest pointer id).
- Java quirks kept: `reset()` on a finger does not clear x/y/downX/downY/pressOrder and `GameplayInput.reset()` keeps `pressCounter` and the
  last-position memory; a duplicate DOWN returns null without touching `ignored`; `release()` always clears `ignored`; `setMaxActive` lowering
  the limit does not drop fingers that are already down.
- Not thread-safe (same as the Java: input and render share a thread).
- CMakeLists.txt: GameplayInput.cpp added to `opsu_core`, `opsu_batch16` test registered.

## Batch 17 (done, DownloadNode PARTIAL): DownloadList (+ Download, DownloadNode data half, Url, HttpClient seam)
> **WIP:** DownloadList, Download and Url are finished; **DownloadNode was only half ported here** (no layout / drawing / animate). Batch 18 added the layout / hit tests / animate half; drawing is still missing. See the WIP table before "Next up".
DownloadList (4.4 KB Java) only needs `Download.isActive/getStatus/cancel` and `DownloadNode.getID/getDownload/clearDownload`, but those two classes
are 14 KB (HTTP + state machine) and 27 KB (mostly card drawing). So this batch ports their LOGIC and leaves the drawing out.
Ported: downloads/DownloadList, downloads/Download (status machine, redirect rules, transfer loop, progress, speed / ETA), downloads/DownloadNode
(data half: fields, ctors, getters / setters, status label / colour, createDownload, toString, `ease` / `smoothing`), downloads/Url (small
`java.net.URL` subset), downloads/HttpClient.h (the network seam), `Utils::copyFile` / `Utils::moveFile`, and ReadableByteChannelWrapper's counter is now
`std::atomic<long long>` (the batch 10 note asked for that once Download was ported).
Tests: tests/test_batch17.cpp (behavioural, no JVM golden: no network, string / state logic). A scripted fake `HttpClient` + fake channels drive the
real code: Url parse / resolve, malformed URL, success (rename + clean, no rename, 0 / 8192 / 8193 bytes), header override and SSL flag, 4 redirects ok
and the 5th refused, 301/302/303/305, no Location, ftp / malformed targets, 307 not followed, unknown length, 404, mid-stream failure, truncated
stream, connect error, no client, unwritable output, cancel while downloading (partial file deleted) and while waiting, destructor with an active
download, speed / ETA (EWMA numbers, `XmYs` vs `XhYmZs`, exact 1000 ms boundary, small-clock first-call branch), DownloadNode (all getters, unicode
fallback, HALF_UP star label, status table, createDownload path / rename / headers / SSL / notifications), DownloadList (every method, duplicate IDs,
consecutive removals). Clean under ThreadSanitizer, AddressSanitizer and UBSan; full cmake suite (19 tests incl. golden 1-4) passes.
Mutation-checked: 70 deliberate bugs in Download / DownloadList / DownloadNode / Url, all caught except 5 behaviourally equivalent ones (resetting
`avgDownloadSpeed` when not downloading, a redundant `rbc->close()` before the scope guard closes it, `isTransferring` without the file check,
`base = downloadURL` vs `resp->url()` in the redirect code (identical for any client that reports the requested URL), the `hasRef_` clear in
`Url::resolve` (every branch sets it again). The first run missed 4 real gaps (speed update at
exactly +1000 ms, the stream not closed on error, `cancelAllDownloads` touching finished downloads, the intermediate `<importDir>/<id>` file); tests added.

Seams / deviations
- **HTTP is a seam** (`HttpClient` / `HttpResponse` in HttpClient.h): exactly what Download used of `HttpURLConnection` (status, `getURL`, header,
  content length, body channel). The client must not follow redirects (Download does, by hand) and `body()` must throw `IOException` for status >= 400.
  Channel `close()` must be callable from another thread and make a blocked `read` throw (that is how `cancel()` stops a transfer). Picking the real
  client (libcurl / Android) is still open. `Options.USER_AGENT` is `Download::USER_AGENT`; `Utils.setSSLCertValidation` is `HttpRequest::disableSSLCertValidation`.
- **Threads**: Download OWNS its worker + cancel threads. `start()` returns `bool` (false = Java null), `join()` waits, the destructor cancels an active
  download and joins. `status` / `contentLength` are atomics, channel / file pointers are behind a mutex, speed / ETA behind another. Clock is injectable.
  `ErrorHandler.error` is the static hook `Download::setErrorHandler` (default `Log::error`). `File` = UTF-8 path string.
- `DownloadList` shares its nodes (`shared_ptr<DownloadNode>`); `get()` is the Java singleton but the constructor is public for tests.
  `DownloadNode` owns its Download (`unique_ptr`; `clearDownload()` destroys it and waits for its thread). Options (`useUnicodeMetadata`, `getImportDir`)
  and `UI.getNotificationManager()` are arguments: `getTitle(useUnicode)`, `createDownload(source, importDir, useUnicode, client, notify)`
  (`DownloadSource` = the 3 `DownloadServer` methods used; the notifier gets `NotifyKind::SUCCESS/FAILURE` for Colors.GREEN / Color.red). The listener
  messages use the title as it was at creation. `getStatusColor()` returns a `StatusColor` enum (GREEN/CYAN/MAGENTA/ORANGE/GRAY) until Colors / Color
  values are ported. Java null strings are "" (titleUnicode / artistUnicode / hash / source); `stars` / `difficulties` are `optional<vector>`.
- **Not ported (rendering / layout)** from DownloadNode: `init`, the result-card hit tests and geometry, `drawResult` and its helpers, `drawResultScrollbar`,
  `clipToResultArea`, `ringColors`, and `animate()` (needs BeatmapSetList; its helpers `ease` / `smoothing` are in).
- `Url` is a subset of `java.net.URL` (header lists what it handles: known protocols, `//authority`, relative / `..` resolution; no percent-decoding or
  IPv6 validation). `MalformedURLException` derives from `java::IOException`, like Java, because Download catches it as one.
- `Utils::moveFile` = `rename`, else copy + delete (silent on delete failure), like `Utils.moveFile`.

**Findings: Java behaviours kept or fixed in Download** (pinned in test_batch17.cpp)
1. *Truncated downloads spun forever in Java.* `FileChannel.transferFrom` returns 0 once the stream ends, and the `while (status == DOWNLOADING &&
   bytesRead < contentLength)` loop just calls it again, so the "N bytes expected, M received" branch right after the loop could never run. **Deliberately
   changed**: the loop stops at end-of-stream, so a truncated download ends as ERROR with that message and `listener.error()`.
2. *An error in the transfer loop ends up as CANCELLED, not ERROR.* The catch sets ERROR and calls `listener.error()`, then the `else` branch of
   `if (status == DOWNLOADING)` overwrites the status with CANCELLED. Kept (the listener still fires; only the final status differs from what the code suggests).
3. `COMPLETE` is published before the file is renamed, so the UI can see COMPLETE while the rename is still pending (and a failing rename flips it to ERROR).
4. 4 redirects are allowed, not 3 (`redirectCount > MAX_REDIRECTS` is tested before the increment). 307 / 308 are not treated as redirects.
5. Responses without a Content-Length (chunked, or > 2 GB) fail with "Negative content length".
6. `updateReadSoFar`'s `else if (lastReadSoFarTime <= 0)` branch is only reachable when the clock is <= 1000, so with a real clock the first call goes through the
   main branch with an enormous time delta and reports "0 B/s" / "?".
7. A `cancel()` that lands before the worker publishes DOWNLOADING is overwritten by it (race in the Java as well).

## Batch 18 (done, DownloadNode STILL PARTIAL): DownloadNode card layout, hit tests, text fitting, animate
> **WIP:** DownloadNode is now about 80 % ported. Still missing (all need the backend): `drawResult`, `drawResultScrollbar`, the real `g.setClip` call, the
> `rect` / `rgba` sprite helpers and `ringColors` (Colors.getDifficultyColor). See the WIP table before "Next up".
Continued the WIP file from batch 17 (it was the only half-ported class whose missing code is mostly plain arithmetic). Ported everything in
DownloadNode that needs no rendering: `init(width, height)` + the list getters (`getListX/Width/Y/Height`, `getBottomReserve`, `maxResultsShown`,
`getButtonOffset`), the shared card geometry (`cardX`, `margin`, `thumbW`, `textLeft`, `textRight`, `btnH`, `btnY`), the hit tests (`resultContains`,
`resultThumbContains`, `resultDownloadContains`, `resultAreaContains`), `getCardRect` (used by the downloads tutorial), the rectangle of
`clipToResultArea` (`resultAreaClip()`), `fit` / `updateFit` (text shortening), the five rectangles of `drawDownloadIcon` (`downloadIconRects`) and
`animate()` with its state getters (`getGlow`, `getPreviewMix`, `getShownProgress`, `getProgressLabel`, ...).
Tests: tests/test_golden5.cpp vs tests/golden/expected5.txt (5.6k lines, regenerate with tests/golden/java/gen5.sh <core/src>): the REAL
DownloadNode.java is compiled against tiny stubs (tests/golden/java/stubs5: fonts with a deterministic width, Graphics that records draws, Download with
settable status / progress, ...) and its private statics / methods / fields are read by reflection. 40 random layouts (font heights, screen sizes down to
123x50 so the `max(buttonOffset, ...)` branch runs), 60 random hit points per layout plus every card / button edge and the float neighbours of each edge
(`nextUp` / `nextDown`), `fit` for 3 fonts x 10 strings (ASCII, kana / kanji, accents) x 25 widths, the icon at 13 button heights (including negative / empty
rects), 60 random `animate` runs of 50 steps (status / progress / library changes, `k` from `smoothing()`), `ease` / `smoothing`, and 30 `updateFit` runs
(unicode toggling, star ratings, repeated calls with cached text, glyph-load calls). Plus tests/test_batch18.cpp for the C++ seams (error before `init`, re-init,
the `DownloadList` lookup overload, COMPLETE / negative progress). Clean under AddressSanitizer + UBSan. Mutation-checked: 30 deliberate bugs in the new code,
all caught except 3 behaviourally equivalent ones (`max(1, ...)` in `maxResultsShown`: `listHeight >= buttonOffset` makes the ceiling >= 1 anyway; rebuilding the
progress label on every frame; not resetting `pctShown` when the download goes away: `pctLabel` always equals `pctShown + "%"`).

Seams / deviations
- **Fonts are a seam**: `CardFonts` (`lineHeight(Font)`, `width(Font, utf8)`, `loadGlyphs(...) const`) for Fonts.LARGE / MEDIUM / BOLD / DEFAULT. The metrics are
  **ints**, like Java's `Font.getLineHeight()` / `getWidth(String)`: the layout arithmetic (`int * 5.9f`, `int + float`) depends on it. `init(w, h, fonts)` remembers the
  reference (not owned; it must outlive later calls), because Java's `btnH()` re-reads the DEFAULT line height on every call. `btnH` / `btnY` / `getCardRect(2, ..)`
  before `init` throw `std::logic_error` (Java: NullPointerException).
- The layout is global static state, as in Java (`init` is called again when the screen size changes). Java's private geometry helpers are public static here: the future
  `drawResult` shares them and the tests read them.
- `DownloadThumbnails.ASPECT` is `DownloadNode::THUMB_ASPECT` = 0.9f (that class is rendering and not ported; gen5.sh fails if the Java value changes).
- `clipToResultArea(Graphics)` becomes `resultAreaClip()` -> `ClipRect{x, y, w, h}` (the `(int)` casts); the backend calls its own `setClip` with it.
- `downloadIconRects(cx, cy, bh)` returns what the five `rect()` calls would draw (those with w > 0 and h > 0, in order); the colour / alpha stays with the drawing code.
- `animate(k, glowTarget, previewTarget, loadedInLibrary, optional<DownloadSnapshot>)`: BeatmapSetList is not ported, so the library lookup is an argument, and the
  download is passed as `{status, progress}` (the overload without the last argument looks it up in `DownloadList::get()` like Java). `getDownloadStatus()` is
  `optional<Status>` (Java null before the first download) and, like Java, keeps the stale value after the download goes away.
- `updateFit(fonts, useUnicode, textW)`: Options.useUnicodeMetadata is an argument. Java compared the cached title / artist Strings by reference; the C++ compares
  values, which rebuilds in the same situations except for two different String objects with equal text (the rebuilt text is identical anyway). `fMore` /
  `fMoreCount` (only `drawResult` uses them) are not ported.
- `fit` operates on UTF-16 units like Java's `substring`, but never splits a surrogate pair (Java could cut one in half; only matters for non-BMP text at a cut).

## Batch 19 (done, small, BeatmapGroup PARTIAL): BeatmapGroup filters + tab layout
> **WIP:** BeatmapGroup is about 80 % ported. Still missing (backend): the tab sprite (`MenuButton`), `contains`, `draw` (`UI.drawTab`). See the WIP table before "Next up".
Picked up from batch 18 ("Next up": the smallest pure-logic file that was left). Ported: beatmap/BeatmapGroup (names / ids / empty messages, `values()` /
`VALUES_REVERSED`, `current()` / `set()` as `currentGroup()` / `setGroup()` because BeatmapSortOrder already owns `opsu::current` / `opsu::set`, the three `filter`s
and the tab layout arithmetic of `init` / `getTabCenterX/Y` / `getTabLeft`).
Tests: tests/test_golden6.cpp vs tests/golden/expected6.txt (768 lines, regenerate with tests/golden/java/gen6.sh <core/src>): the REAL BeatmapGroup.java and
Long2.java are compiled against tiny stubs (tests/golden/java/stubs6: BeatmapSet / BeatmapSetNode / Beatmap / MenuButton / UI / GameImage / Image). 400 random
libraries (0-90 sets, 8 styles of "last played" values: many ties and zeros, within 2^31 ms, days apart, far apart, multiples of 2^32, near Long.MAX_VALUE, sets with no
beatmaps), each filtered by all 3 groups; 300 random tab layouts (small / medium / wide tabs, both branches of the spacing rule) and 60 targeted ones with
`tabOffset == tabWidth` exactly (containerWidth = 6 x tabWidth, +-1). Clean under AddressSanitizer + UBSan. Mutation-checked: 21 deliberate bugs, 20 caught on the first
run; the survivor `tabOffset > tabWidth` -> `>=` (random layouts never hit equality) was closed by the targeted cases. The only mutant that still survives is `right < n` ->
`right <= n` in the heap's siftDown, whose only effect is an out-of-bounds read (UB), not a different result.

Seams / deviations
- BeatmapSetNode / BeatmapSet are not ported, like in batch 6 the filters work on the sets' beatmaps (`BeatmapSetBeatmaps`) and **return indices into the input list**
  (`filter(g, vector<const BeatmapSetBeatmaps*>)`). A template overload `filter(g, nodes, getSet)` returns the nodes themselves for when BeatmapSetNode exists.
- **RECENT returns java.util.PriorityQueue iteration order, which is the heap array, NOT sorted.** The queue is rebuilt with the JDK's exact siftUp / siftDown, so the
  order (also for equal timestamps) is identical to Java's. The song menu must not assume the result is sorted (it is re-sorted by the sort order afterwards in Java as well).
- `isFavorite(set)` = `BeatmapSet.isFavorite()`; `lastPlayed(set)` = the private Java helper (0 when no beatmap was ever played).
- The tab is just its centre: `init(g, containerWidth, bottomY, tabWidth, tabHeight)` takes the size of `GameImage.MENU_TAB`'s image. `getTabLeft` uses the width given to the
  last `init` (Java re-reads the image on every call; `init` is re-run when the skin changes, so they only differ in between). Tab accessors throw `std::logic_error`
  before `init` (Java: NullPointerException).

**Finding: a real bug in the Java RECENT filter (kept on purpose, pinned in golden6).** Its comparator is `Long2.compare(a, b)`, and `fluddokt.opsu.fake.Long2.compare` is
`(int) (i - j)`, which truncates a 64-bit difference to 32 bits. "Last played" values are epoch milliseconds, so two sets played more than 2^31 ms (about **24.8 days**) apart
overflow and compare the wrong way round (and exactly 2^32 ms apart compares as equal). For a library played over more than ~25 days the PriorityQueue is no longer a
valid heap order of "oldest first", so the "Last Played" tab can drop recent sets and keep old ones. The same helper is used by `BeatmapSortOrder.DATE` (batch 6: the
"Date Added" sort has the same flaw). Fix = use `Long.compare` (Java) / a plain three-way compare (C++); it is one line in `Long2.java` and `java::long2Compare`, but it
changes behaviour, so it is NOT done here. Say the word and I will fix it in both and regenerate expected6.txt (and the batch 6 test).

## Batch 20 (done, tiny): JSON reader (org.json subset) + Double.parseDouble / Double.toString
Groundwork for the download servers (the next "Next up" item): every server reads its search results with `org.json`, and nothing in the C++ port parsed
JSON yet. I did NOT start `DownloadServer` itself in this batch (12 KB; it is the next slice, see "Next up").
Ported: util/Json (`JSONObject`, `JSONArray`, `JSONException`: parse, `has/length/keys/isNull`, `getString/getInt/getDouble/getBoolean/getJSONObject/getJSONArray`,
the matching `opt*` with defaults, `optJSONObject/optJSONArray` returning nullptr like Java null, compact `toString`), and in util/JavaCompat `java::parseDouble` and
`java::doubleToString` (`parseFloat` / `floatToString` are now thin wrappers over the same templates, behaviour unchanged).
Tests: tests/test_golden7.cpp vs tests/golden/expected7.txt (19k cases, regenerate with tests/golden/java/gen7.sh, needs a JDK 19+): the running JDK's
`Double.toString` on 11k doubles (random bit patterns, short decimals, wide exponents, one-digit values, the 1e-3 / 1e7 switch points, MIN/MAX, NaN, inf, -0) and
`Double.parseDouble` on about 8k strings (suffixes d/f, whitespace, NaN / Infinity, overflow / underflow, rounding ties, malformed). All match.
tests/test_batch20.cpp (behavioural): the JSON accessor rules below. Clean under AddressSanitizer + UBSan. Mutation-checked: 21 deliberate bugs in Json.cpp, 17
caught on the first run; the survivor "a high surrogate followed by a non-low-surrogate escape" got a test, and the rest are behaviourally equivalent (`01` is still an
error through the object parser, the Integer-vs-Long boundary at -2^31 is not observable through any accessor, and one mutation edited a default that lives in the header).

> **ASSUMPTION to confirm:** the core has no org.json source or dependency declaration (it is not in core/build.gradle and no jar was available here), so there was
> nothing to run as a golden. The accessor rules follow the stleary org.json (2018-2020): `getString` is strict (a number is NOT converted), `optString` converts,
> `getInt` accepts a number or an `Integer.parseInt`-able string, `optX` = try `getX` else default. **Android's built-in org.json is more permissive** (its `getString`
> converts numbers, `optInt("12.0")` is 12, ...). If the app really runs against the Android one, tell me and `getString` / string-to-int coercion should be switched;
> the servers only call `getString` on real strings, so it matters only for odd API responses.

Seams / deviations (also in the Json.h header)
- **Strict JSON on purpose.** org.json's JSONTokener is very lenient (single quotes, unquoted keys / strings, `=` `=>` `;` separators, `[1,,2]`, `[1,]`, leading zeros,
  comments); here those are `JSONException`. The servers talk to JSON APIs that emit valid JSON.
- Numbers: Integer if it fits, else Long, else Double (Java: BigInteger / BigDecimal); `-0` is a Double -0.0; a double that overflows to Infinity becomes the text as a
  string (an org.json quirk, kept). getLong / optLong, put / remove / names and the rest of the API are not ported (unused).
- Nesting deeper than 512 throws (Java would overflow the stack); a lone `\uD800-\uDFFF` escape becomes U+FFFD (a Java String could hold the lone surrogate);
  keys are kept sorted (HashMap order is unspecified); duplicate keys and a missing '{' / '[' throw like Java; text after the top-level value is ignored like Java.
- `optJSONObject` / `optJSONArray` return raw pointers owned by the parent (valid while the parent lives). Strings are UTF-8 like everywhere in this port.
- Not ported on purpose: hex-float parsing in `parseDouble` (Java accepts `0x1p3`; the existing `parseFloat` never did), and `Utils.readJsonObjectFromUrl` /
  `DownloadServer.readJsonArray` (they need `HttpClient`; they come with DownloadServer).

## Batch 21 (done, tiny): java.net.URLEncoder
Second groundwork piece for the download servers (batch 20 was the JSON reader). I did NOT start `DownloadServer` or any server class in this batch.
Ported: `java::urlEncode(s)` in util/JavaCompat = `URLEncoder.encode(s, "UTF-8")` (`[A-Za-z0-9]` and `. - * _` stay, space -> `+`, everything else, `~` included,
-> upper-case `%XX` of the UTF-8 bytes). Used by 10 of the 11 servers; MnetworkServer additionally does `.replace("+", "%20")` on the result (idiom covered in a test).
Tests: tests/test_golden8.cpp vs tests/golden/expected8.txt (6075 cases from the running JDK, regenerate with tests/golden/java/gen8.sh: fixed strings for every
ASCII punctuation char, controls, NUL, each UTF-8 length boundary, emoji, CJK / Cyrillic / Arabic / Hangul, then 6000 random mixed strings), all match.
tests/test_batch21.cpp (behavioural): malformed UTF-8 and the Mnetwork idiom. Clean under AddressSanitizer + UBSan; golden / golden7 / batch20 re-run, unchanged.

Deviation: C++ strings are UTF-8 bytes and can be malformed, a Java String cannot. Malformed input is first run through `decodeUtf8Lossy`, so a bad byte comes out as
`%EF%BF%BD` (U+FFFD). Java would never see such input (a lone surrogate would give `%3F`); no caller can produce either, since every query comes from UI text.

## Batch 22 (done, small): SimpleDateFormat slice for the download servers (util/JavaDate)
Last groundwork piece before `DownloadServer` (after the JSON reader in batch 20 and `URLEncoder` in batch 21). I did NOT start `DownloadServer` or any server class.
Ported: util/JavaDate.h/.cpp: `java::parseDate(text, pattern, utc)` (lenient `SimpleDateFormat.parse` for patterns of y/M/d/H/m/s/S, literals and 'quoted' text),
`java::formatDate(ms)` (= `new SimpleDateFormat("d MMM yyyy HH:mm:ss", Locale.US).format(new Date(ms))`, in the default zone) and `java::reformatDate(s, pattern, utc)`
(= the whole `formatDate(String)` of a server: reformat, or return the raw string on ParseException). Covers the 4 string-date servers (Bloodcat `yyyy-MM-dd HH:mm:ss.SSS`
in the default zone; Catboy / Hinamizawa / Nekoha `yyyy-MM-dd'T'HH:mm:ss` in UTC after their own 19-char cut, which stays in the server code; OsuMirror
`yyyy-MM-dd'T'HH:mm:ss'Z'` in UTC) and YaSOnline (`formatDate(int seconds)` = `formatDate(seconds * 1000LL)`).
Tests: tests/test_golden9.cpp vs tests/golden/expected9.txt (88,398 cases from the running JDK 21, regenerate with tests/golden/java/gen9.sh; 6.4 MB): per time zone
(UTC, Asia/Jakarta, America/New_York, Europe/London, Australia/Lord_Howe, Asia/Kolkata, Pacific/Chatham, America/Sao_Paulo, Asia/Tehran; the checker switches `TZ`)
random dates, wall times around DST gaps and overlaps, ~85 malformed / lenient inputs (month 13, day 0 / 32, hour 24 / 99, `-5` fields, blanks and tabs, trailing text,
missing parts, `+` signs, exponent text, leading zeros, 5-digit years) x 3 patterns, 400 random fragment mixes, and 1,200 random `format` instants per zone.
tests/test_batch22.cpp (behavioural). Clean under AddressSanitizer + UBSan. Golden / golden7 / golden8 / batch20 / batch21 re-run, unchanged.
Result: **0 mismatches in scope**; 13,307 cases are reported as "out of scope" and counted, never dropped (see below). Mutation-checked: 11 deliberate bugs, 9 caught on the
first run (month rollover, blank skipping, minus sign, milliseconds, day-1, hour wrap, trailing text, literal matching, DST overlap order); the 2 survivors are equivalent in
scope (`%04lld` -> `%lld`: no in-scope year has fewer than 4 digits; `tm_isdst = 0` for the gap: the overlap re-check lands on the same instants).

Found by the golden and fixed: **DST overlap.** A wall time that occurs twice (e.g. New York 2023-11-05 01:30) resolves to the *later* instant in Java, glibc `mktime` picks the
earlier one; `parseDate(utc=false)` now tries the offsets a day before / after and keeps the latest instant whose wall clock matches. The DST *gap* already agreed.

Deviations / out of scope (the golden classifies these instead of hiding them; the header of JavaDate.h lists them too)
- **Results before 1900-01-01** are not Java-exact: `GregorianCalendar` is Julian before 1582-10-15 (the port is proleptic Gregorian), and before 1900 zones differ (Java keeps a fixed
  first offset, libc uses local mean time, e.g. Jakarta +7:07:12). Years <= 0 / negative years (BC in Java) are the same story. Real API dates are 2007+.
- Numbers longer than 9 digits are a parse failure here (Java wraps them through `Long.intValue()` / saturates a Double); `DecimalFormat` also reads `NaN`, the infinity sign and
  non-ASCII digits (`Character.digit`) as numbers, this port reads none of them (it returns the raw string, Java would give a date). None of this can come from a JSON date.
- Patterns other than numeric y / M / d / H / m / s / S throw `std::invalid_argument` (no `MMM` parse, no two-digit `yy`, no `E` / `a` / `z`); nothing in the core needs them.
- The default zone is the process's (`TZ`, libc `localtime_r` / `mktime`), like Java's default zone comes from the OS. Needs `tm_gmtoff` (glibc, bionic, macOS have it).

> **ASSUMPTION to confirm:** `formatDate` always prints **English** month names (`Locale.US`). The Java servers that use the default locale (Bloodcat, OsuMirror, YaSOnline) would print
> the device language's names (e.g. "Mei" / "Agu" / "Okt" / "Des" on an Indonesian device); Catboy / Hinamizawa / Nekoha already pass `Locale.US`. If you want the device-language
> names back, say so; it needs a month-name table per locale (or an ICU / platform call) and is a one-line seam in `formatDate`.

## Batch 23 (done, small): DownloadServer (the base class only)
Ported: downloads/DownloadServer.h/.cpp, the abstract base of the 11 download servers (12 KB Java): the constants, `resultList(status)` default,
`lookupSet`, `readJsonArray` / `readJsonObject` (over the `HttpClient` seam), `parseSet`, `applyStars`, `applySetInfo`, `getPreviewURL` and the
default `getDownloadRequestHeaders` / `isDownloadInBrowser` / `disableSSLInDownloads`. It implements `DownloadSource`, so a server can go straight into
`DownloadNode::createDownload`. **No concrete server was ported in this batch (state at the time). Current state: all 4 live servers are done (Catboy 25, OsuDirect 26, Hinamizawa 27, Nekoha 28); the other 7 are dead, see "Intentionally not ported: dead download servers".**
Tests: tests/test_batch23.cpp (behavioural; a fake `HttpClient` and a fake server): full / partial / malformed set JSON, every status word incl. case and
the two non-ASCII folds, difficulty sorting (stable, 90 ties), od / hp key fallbacks, request settings (timeouts, headers), every non-200 / non-JSON /
malformed body, IOException and MalformedURLException propagation, `lookupSet` endpoint-then-search paths. Clean under AddressSanitizer + UBSan.
Mutation-checked: 10 deliberate bugs (status check, trim bound, sort stability, rating `< 0`, Kelvin fold, lookup id check, fallback status, od fallback,
default overload, timeouts); 9 caught on the first run, the survivor (`stable_sort` -> `sort`) was invisible with 4 elements (libstdc++ uses insertion sort
below 17), so the 90-element tie test was added and now catches it.
NOT a golden test: unlike batches 15 / 20 / 22 this was not run against the real Java (the real class needs the real `DownloadNode`, org.json and
`HttpURLConnection`). The logic is a line-by-line port over the golden-verified JSON reader (batch 20); a golden suite with stubs is possible if you want one.

Seams / deviations
- `readJsonArray` / `readJsonObject` are protected MEMBERS (Java: static) using the `HttpClient*` given to the constructor (non-owning; nullptr until a request is
  made, then `std::logic_error`). Same request as Java: connect timeout 10 s, read timeout 15 s, `User-Agent: opsu!`, `Accept: application/json`.
- The response body is raw UTF-8 bytes: Java's `new String(bytes, "UTF-8")` would replace invalid sequences with U+FFFD, this keeps them (only a broken server could tell).
- Nullable results: `DownloadNode[]` -> `std::optional<NodeList>` (`vector<unique_ptr<DownloadNode>>`, elements may be null); null node -> empty `unique_ptr`;
  null URL / search text -> `std::optional<std::string>`; `lookupSet` moves the matching node out of the result list.
- The two `resultList` overloads (bool / int): a subclass overriding one must add `using DownloadServer::resultList;` (C++ name hiding).
- `equalsIgnoreCase` (status words) is ASCII-insensitive plus the two non-ASCII characters Java folds into the words' letters (U+212A -> k, U+0131 -> i).

## Batch 24 (done, tiny): DownloadServer::reportError (RippleServer ported, then REMOVED; SSL flag added, then REMOVED)
I first ported RippleServer, then learned it is abandoned and deleted it (files, CMake entry, tests). The per-server SSL flag I had added for it (`setSSLCertValidation`) was then kept
"for YaSOnline", but YaSOnline is not in the Java build either (see "Intentionally not ported"), so that went too: no live server calls `Utils.setSSLCertValidation`. What stays:
- `ErrorHandler.error(msg, e, report)` is a protected `DownloadServer::reportError`, forwarding to a new public `Download::reportError` (it was a file-static function in Download.cpp
  behind the same `setErrorHandler` hook; behaviour unchanged). All four live servers (OsuDirect, Catboy, Nekoha, Hinamizawa) call it from their `resultList` catch block.
Tests: tests/test_batch24.cpp (behavioural): `reportError` / `Download::reportError` reach the handler with message / exception / flag, a null exception, the default handler does not crash.
Mutation-checked: 2 deliberate bugs (report flag flipped, message changed), both caught. tests/test_batch23.cpp still passes. Nothing else was touched.

## Batch 25 (done, tiny): CatboyServer (1 of the 4 live download servers)
Ported: downloads/servers/CatboyServer.h/.cpp (5.6 KB Java): name, download URL, set lookup URL (`/api/v2/s/<id>`), both `resultList` overloads (`bool rankedOnly` and `int status`),
unicode-name handling, date reformatting, the paging estimate, `minQueryLength` 0. It is a `DownloadServer`, so it goes straight into `DownloadNode::createDownload` and `lookupSet`
uses its set endpoint first. **State at the time: only Catboy. OsuDirect was ported in batch 26, Hinamizawa in batch 27, Nekoha in batch 28 (all 4 live servers done).**
Tests: tests/test_batch25.cpp (behavioural; fake `HttpClient`): exact request URLs (query / sort encoding, `limit`, `offset` for pages 0 / 1 / 2 / 3, `&status=` for ALL / RANKED / LOVED /
0 / -1, the `bool` overload), every parsed field, the unicode rules (empty / equal / JSON null / missing / a number), dates (fractional seconds and offsets dropped, 18-character date,
the 20th character cut, unparseable -> the RAW string not the prefix, missing field, UTC -> default zone), the paging estimate (19 / 20 / 21 / empty page), every null path resets the
total to -1, a JSONException mid-list (7 variants) returns the half-filled list and leaves the total, IOException / no client propagate, `lookupSet` (endpoint hit, endpoint answers
another id -> search fallback, endpoint 404). Clean under AddressSanitizer + UBSan. Mutation-checked: 25 deliberate bugs; 18 caught on the first run, the survivors were
- 6 behaviour-neutral (`titleU == title` / empty `artistU` / JSON null `title_unicode` all end up as the plain name; `>= 19` vs `> 19` units gives the same prefix; 2 sed no-ops), and
- 1 real gap (the 19-unit cut limit, e.g. "...:38:471" would parse as 471 seconds): a test was added and now catches it.
NOT a golden test (not run against the real Java), same as batches 23 / 24.

Seams / deviations
- Same as DownloadServer (batch 23): `readJsonArray` goes through the `HttpClient`, nullable results are `std::optional` / empty `unique_ptr`, `using`-free because both `resultList` overloads are overridden.
- `item.optString("title_unicode", null)`: null / missing / JSON null -> "" (DownloadNode treats null and empty Unicode names alike). A number is converted to text, like org.json.
- The Java returned a half-filled `DownloadNode[]` on a mid-list JSONException (unfilled elements null, total result count unchanged); same here (empty `unique_ptr` elements).
- Java evaluation order is kept (`title`, `artist`, the two optional names, `id`, `creator`), so a missing field stops the loop at that element.
- `(page - 1) * PAGE_LIMIT` and `nodes.length + offset` wrap like Java ints (page 0 -> offset -20).
- `formatDate`: `s.substring(0, 19)` is done on UTF-16 units (whole code points only; a cut surrogate pair cannot parse in either implementation); parse failure returns the original `s`, not the cut-down string.
  Parsed as UTC, printed in the default time zone (`TZ` here) with English month names (`Locale.US`).
- `ErrorHandler.error` is `DownloadServer::reportError` (then `Log::error(e)` as in Java); only reachable for a MalformedURLException, which the built URL never causes.

## Batch 26 (done, tiny): OsuDirectServer (2 of the 4 live download servers)
Ported: downloads/servers/OsuDirectServer.h/.cpp (5.6 KB Java, used by `DownloadsMenu` + `SongInfoHeader`). It is its own server, not Catboy: the Java has the same logic as CatboyServer (batch 25) with different constants; a diff of the two Java files
shows only: name `osu.direct`, download URL `https://osu.direct/api/d/<id>` (Catboy: `/d/<id>`), lookup URL `https://osu.direct/api/v2/s/<id>`, search URL `https://osu.direct/api/v2/search?mode=0&query=..&sort=..&amount=20&offset=..`
(**`amount=`, where Catboy uses `limit=`**). Everything else (both `resultList` overloads, unicode-name handling, `formatDate`, the paging estimate, `minQueryLength` 0, the JSONException half-filled list, `reportError`) is line-for-line the same,
so the C++ is the CatboyServer port with those four strings changed. It is a `DownloadServer`, so it goes straight into `DownloadNode::createDownload` and `lookupSet` uses its set endpoint first.
Tests: tests/test_batch26.cpp = the batch 25 suite retargeted (exact request URLs incl. `amount=`, offsets for pages 0 / 1 / 2 / 3, `&status=` values, every parsed field, unicode rules, dates, paging estimate, null paths,
mid-list JSONException, IOException / no client, `lookupSet`, `DownloadSource` use). Clean under AddressSanitizer + UBSan. tests/test_batch23.cpp and test_batch25.cpp still pass.
Mutation-checked: 6 deliberate OsuDirect-specific bugs (`amount` -> `limit`, download URL `/api/d/` -> `/d/`, server name, lookup URL, sort key, paging `+2`), all 6 caught on the first run.
NOT a golden test (not run against the real Java), same as batches 23 / 24 / 25. Nothing else was touched (only CMakeLists.txt: source + test registration).

Seams / deviations: identical to CatboyServer (see batch 25).

## Batch 27 (done, tiny): HinamizawaServer (3 of the 4 live download servers)
Ported: downloads/servers/HinamizawaServer.h/.cpp (5.8 KB Java, used by `DownloadsMenu`). OsuDirect (batch 26) and Catboy are separate servers (own name, URLs, `amount=` vs `limit=`) whose Java code has the same logic. Hinamizawa has its own logic instead, so it is not a copy of either. What it does differently:
- name `Hinamizawa`, download URL `https://mirror.hinamizawa.ai/api/v1/hinai/d/<id>`, search URL `https://mirror.hinamizawa.ai/api/v2/search?mode=0&q=<enc>&amount=20&offset=<n>` (**`q=`**, no `sort`), then `&status=<n>`
  if not ALL, then **only for an EMPTY query** `&sort=updated:desc` (ALL) / `&sort=ranked_date:desc` (other), appended unencoded (the other servers always send an encoded `sort`);
- **no set lookup endpoint**: `getLookupURL` is the base default (nullopt), so `lookupSet` goes straight to the search fallback;
- entries are parsed by a private static `parseItem` that returns null on any JSONException (missing / wrong-typed `title`, `artist`, `id`, `creator`): the odd entry is **skipped** and the rest of the page stays
  (the other servers keep the half-filled list instead); the paging estimate uses the RAW array length with `>= PAGE_LIMIT` (not the kept count, not `==`), so a page of 20 skipped entries still reports 21;
- the only exception `resultList` catches is MalformedURLException (-> `reportError` + `Log::error`); there is no JSONException catch in `resultList`.
Tests: tests/test_batch27.cpp (behavioural; fake `HttpClient`): exact URLs (empty / non-empty query x ALL / RANKED / LOVED / 0 / -1, pages 0 / 1 / 2 / 3), every parsed field, unicode rule, bad date -> raw string, skipped
entries (3 kinds) keep order, estimate for 19 / 20 / 21 raw entries and for 20 skipped entries, null paths reset the total to -1 (404 / not JSON / JSON object), IOException / no client, `lookupSet` search-only path
(no endpoint request, no search text -> nothing), `DownloadSource` use. Clean under AddressSanitizer + UBSan. batches 23 / 25 / 26 still pass.
Mutation-checked: 11 deliberate bugs; 10 caught on the first run. The survivor (`catch (JSONException)` widened to `catch (runtime_error)` in `parseItem`) is behaviour-neutral: `JSONException` derives from `std::runtime_error` and nothing else runtime_error-typed is thrown there.
NOT a golden test (not run against the real Java), same as batches 23-26.

Seams / deviations: as CatboyServer (batch 25), plus
- **Java quirk kept (probably a Java bug, see "Next up" note):** `arr.optJSONObject(i)` returns null for an element that is not a JSON object (number, null, array, string), and `parseItem(null)` then throws a **NullPointerException**,
  which neither `parseItem` (catches JSONException only) nor `resultList` catches. So one such element does NOT skip the entry, it aborts the whole call. In C++ this is a `std::logic_error` that propagates; `totalResults()` keeps its previous value.
  Tested. If the Java is fixed (`if (item == null) continue;`) change `parseItem` to return nullptr and flip that test.

## Batch 28 (done, small): NekohaServer (4 of 4 live download servers: the download servers are finished)
Ported: downloads/servers/NekohaServer.h/.cpp (6.5 KB Java, used by `DownloadsMenu`). Its own logic again (not a copy of Catboy / OsuDirect; the "skip odd entries" parser is the same as Hinamizawa's):
- name `Nekoha`, download URL `https://mirror.nekoha.moe/api/download/<id>`, **lookup endpoint** `https://mirror.nekoha.moe/api/beatmapset/<id>` (so `lookupSet` tries it first);
- the API answers a JSON **object** (`readJsonObject`) with a `beatmapsets` array of up to 100 sets per SERVER page; a UI page of 20 is a slice: `offset = (page - 1) * 20`, `apiPage = offset / 100 + 1`,
  `start = offset % 100` (Java truncating `/` and `%`, so a negative offset gives `start < 0`), `end = min(start + 20, length)`;
- **the last server page is cached** (`cachedKey_` = the full search URL, `cachedSets_`), so UI pages 1-5 are one request and page 6 is the next; a different query / status / server page replaces the cache; a failed
  request (null body, no `beatmapsets` array) returns null with total -1 and leaves the old cache alone;
- search URL `https://mirror.nekoha.moe/api/search?mode=osu&q=<enc>&page=<n>`, then `&status=ranked,approved` (RANKED) / `&status=loved` (LOVED) / nothing for any other status, then **only for an empty query**
  `&sort=updated&order=desc` (ALL) / `&sort=ranked&order=desc` (any other status, incl. 0 / -1);
- total estimate = `offset + keptNodes + (more ? 1 : 0)` with `more = end < length || length >= 100` (kept nodes, unlike Hinamizawa's raw length).
Tests: tests/test_batch28.cpp (behavioural; fake `HttpClient`): all URL shapes (12 query / status / page combos, bool overload), slicing and the cache across 100-set / 30-set server pages (requests counted), estimates (10 / 20 / 30 / 100
sets, start past the end), parsing of every field, skipped entries and the kept-node estimate, 6 null paths that keep the cache, the NullPointerException quirks (below), IOException / no client, `lookupSet` (endpoint, then search
fallback, then nothing), `DownloadSource` use. Clean under AddressSanitizer + UBSan. Batches 23-27 still pass.
Mutation-checked: 24 deliberate bugs (every URL piece, apiPage, negative start, both status words, sort rules, cache on / key / not-cleared-on-failure / replacement, `beatmapsets` key, `>=`, `<`, kept vs raw count, `+2`, skip,
NPE, slice bound), all 24 caught. (The first attempt hit the time limit because each run rebuilt the whole library; the runs were redone against pre-built objects.) NOT a golden test (not run against the real Java), as 23-27.

Seams / deviations: as CatboyServer / HinamizawaServer, plus
- **Java quirks kept:** a non-object element inside the slice, and **page 0 or any page <= 0** (`start` is negative, `optJSONObject(-20)` is null), make `parseItem(null)` throw a NullPointerException that nothing catches.
  In C++ that is a `std::logic_error`; the total keeps its value and the server page IS already cached (the next call for page 1 needs no request). Elements outside the slice are never looked at.
- Offsets wrap like Java ints (`(uint32_t)` arithmetic); `/` and `%` are C++ truncating like Java's.
- `cachedSets_` holds a COPY of the `JSONArray` (Java kept a reference; no observable difference, nothing mutates it).

## Batch 29 (done, small): BeatmapSet
Ported: beatmap/BeatmapSet.h/.cpp (8 KB Java): the sort (`LAZER_ORDER`: star rating with the current mods ascending, unrated first, then `Beatmap.compareTo`, then version
ignoring case), `size/get/remove/iteration`, `getInfo` (the 5 song-menu lines), `toString`, `matches(query)`, `matches(type, op, value)` (ar/cs/od/hp/bpm/length/star(s)),
`isFavorite/setFavorite/isPlayed`. This is the piece BeatmapGroup / BeatmapSortOrder were waiting on (they still use `std::vector<Beatmap>` = `BeatmapSetBeatmaps`; `BeatmapSet::beatmaps()` hands that over).
Tests: tests/test_batch29.cpp (behavioural): ordering (unrated first, tie keys, case-insensitive version, stability on 90 elements, mod stars of the current mods), access + out_of_range, `getInfo` (exact
strings incl. `DecimalFormat("##.#")` HALF_EVEN, 0 prints "0", BPM range / "--", minutes not wrapped at 60, DT speed, HR x1.4 and the cap at 10), `matches` both forms (every field, first-beatmap-only
fields, int division for `length`, `star` rounds the RAW rating), favourite hook. Clean under AddressSanitizer + UBSan. Mutation-checked: 12 deliberate bugs; 9 caught first time, 2 gaps found (std::sort instead
of stable_sort needs > 16 elements; the cap at 10) and covered by new tests, 1 survivor is behaviour-neutral (`if (changed)` only skips rebuilding an identical vector).
NOT a golden test: there is no JDK (javac) in this environment, so it was not run against the real Java. A golden driver can be added later with `tests/golden/java/gen*.sh` (needs a JDK 19+).

Also fixed (build file, found while registering the test): in CMakeLists.txt the line `add_test(NAME opsu_batch26 COMMAND opsu_test_batch26 test_batch27 test_batch28)` passed the names
of the batch 27 and 28 tests as ARGUMENTS to batch 26, so **`ctest` never ran batch 27 / 28** (the executables were built). Now `opsu_batch26`, `opsu_batch27`, `opsu_batch28`, `opsu_batch29` are separate tests.

Seams / deviations (also in BeatmapSet.h)
- Options / DB are explicit: `ModParams current` (constructor, `sortByDifficulty`, `getInfo`), `useUnicodeMetadata`, and `BeatmapSet::setFavoriteStatusHook` = `BeatmapDB.updateFavoriteStatus` (no hook = nothing written).
- `String.toLowerCase()` in `matches(query)` is **ASCII-only** (Java folds all Unicode); the caller must lower-case the query the same way.
- `sortByDifficulty` reports "changed" by comparing the permutation (Beatmap has no `equals`, so Java compared identity). `std::stable_sort` = TimSort's stability; Java could throw
  "Comparison method violates its general contract" for an inconsistent comparator (int-wrapping `compareTo`), this just gives an unspecified order.
- `IndexOutOfBoundsException` is `std::out_of_range`; `toString` / `matches(query)` on an empty set throw it too (Java `get(0)`).

## Batch 30 (done, tiny): BeatmapSearch (the static search half of BeatmapSetList)
Ported: beatmap/BeatmapSearch.h/.cpp = `BeatmapSetList.normalizeQuery`, the query splitter (`split("\\s+")` + the `SEARCH_CONDITION_PATTERN` conditions ar/cs/od/hp/bpm/length/star(s) with = == > >= < <=)
as `parseQuery`, and the static `filter` as the template `filterNodes(query, source, getSet, cancelled)`. This is **only the static part of BeatmapSetList**: the class itself (node lists, `expand` /
`unexpand`, `addSongGroup`, `deleteSong*`, `search` / `applySearch` / `SearchSnapshot`, ID / hash maps, `init`) is NOT ported yet (it needs BeatmapSetNode), see the WIP table.
Tests: tests/test_batch30.cpp (behavioural, no JDK here): normalize (trim of every char <= U+0020, case), Java `split` rules (leading "" for a width>0 match at 0, trailing empties dropped, "" -> [""], \x0B and \f
are `\s`), every condition operator, `find()` semantics (pattern inside a term, `9.5.5`, `ar=>5`, `ar>=x` are text), terms then conditions (AND), early empty return, cancellation points (i = 0, 256, 512; nullopt = Java null),
another node type. Clean under AddressSanitizer + UBSan. Mutation-checked: 10 deliberate bugs, all real ones caught (one gap, the cancellation index, got a test); the 2 survivors (dropping the
"current is empty" early returns) are behaviour-neutral: an empty list stays empty through `keep`. Not golden (no JDK).
Seams / deviations: see BeatmapSearch.h (nodes = template + `getSet`, `latest.get() != seq` = `cancelled()`, ASCII-only `toLowerCase`, `std::regex` ECMAScript for the condition pattern which behaves like java.util.regex for it).

## Batch 31 (done, small): BeatmapSetNode, data + text half (PARTIAL)
Ported: beatmap/BeatmapSetNode.h/.cpp: the node data (`beatmapIndex`, `index`, `prev` / `next`, `panelHover` / `rowHover` / `select`, the layout constants `MAX_SPREAD_ICONS`, `CHIP_SCALE`, `ROW_HEIGHT_SCALE`,
`PANEL_BAR_TOP` / `PANEL_BAR_H`, `SLICE_LEFT` / `SLICE_RIGHT`), `getSelectedBeatmap`, `getInfo`, `toString`, the text helpers `truncate` / `fit` (+ the `TextFit` cache) and the cached rating text (`"%.2f"`).
**Not ported (needs the graphics backend, listed in the WIP table):** `drawSetPanel` (both overloads), `drawDifficultyRow` (both), `drawFlash`, the sliced / solid panel images (`getSolidPanel`, `makeSolidMask`, `drawSliced`),
`drawStarPill`, `fillCircle`, `fillRounded`, `aspect`, `withAlpha`.
Tests: tests/test_batch31.cpp (behavioural, no JDK here): defaults and constants, selected beatmap (valid / too large / negative; a pointer into the set, not a copy), shared set, `getInfo` / `toString` (index -1, valid,
out of range, -2), `truncate` (exact fit, one off, empty, the `len > 1` floor, negative width, 1 / 2 / 3 / 4-byte code points, a broken sequence), the `fit` cache (hit does not measure; width / font / text change
re-measure; the prefix is not part of the key; prefix is included in the truncation), rating text (HALF_UP). Clean under AddressSanitizer + UBSan. Mutation-checked: 12 deliberate bugs; 9 caught first time, 1 gap
(the 4-byte lead byte, needed a case that stops at one emoji) got a test; the 2 survivors are behaviour-neutral (always re-formatting the rating gives the same text; removing `beatmapIndex < 0 ||` still
yields nullptr because the negative index wraps to a huge size_t). Not golden (no JDK).
Seams / deviations: see BeatmapSetNode.h (shared_ptr to the set, raw `prev` / `next`, Options explicit, `TextMeasure` + font identity pointer instead of `Font`, `truncate` cuts whole code points where Java cut UTF-16
units, no null strings, the `fit` cache ignores the prefix like Java).

## Batch 32 (done, small): BeatmapSetList core (PARTIAL: lists, init, expand / unexpand, navigation)
Ported: beatmap/BeatmapSetList.h/.cpp: `reset` (current group's filter over `parsedNodes`), `size`, `addSongGroup` (+ `mapCount`, the set-ID table `containsBeatmapSetID`), `getMapCount` / `getMapSetCount`, `getBaseNode`, `getRandomNode`,
`getNode`, `getExpandedIndex`, `expand`, `unexpand` (private), `init` (stable sort by `BeatmapSortOrder::current()`, indices and prev / next links).
**Not ported yet (WIP table):** `deleteSongGroup` / `deleteSong` (delete files + BeatmapDB), `search` / `applySearch` / `SearchSnapshot` / `isCurrentQuery` / `lastQuery` (and with it the separate `nodes` list that points at a search result;
here `nodes` and `groupNodes` are one list, as in Java until a search happens), the MD5 hash table + `getBeatmapFromHash` (see the design note below), the Java static `create` / `destroy` / `get` singleton.
Tests: tests/test_batch32.cpp (behavioural, no JDK here): counts, set-ID table (0 and negative never stored), empty list, `reset` + `init` (sort, links, indices, prev of the first / next of the last), group filter in `reset`, the global sort
order, `getNode` incl. the negative-shift quirk, `expand` of the first / middle / last node, forward AND backward chains, the base list untouched, expanding another set undoes the previous one, a bad index undoes and expands nothing, 100 repeated
expansions, `init` while expanded, random node (`Math.random` seam; the expanded group node gives `next`), stability for 60 ties (> 16 elements). Clean under AddressSanitizer + UBSan. Mutation-checked: 24 deliberate bugs;
18 caught first time, 3 gaps found and covered (stable sort needs > 16 ties, `init` must reset the first node's `prev`, the retired-expansion cap), 3 survivors are unreachable today: `eCur->index = expandedIndex_` (the base node's index is
never changed by `expand`) and the two `expandedIndex_ >= size()` guards (nothing can shrink the list while expanded until delete is ported; re-check them when `deleteSong*` is ported).
Not golden (no JDK).

Seams / deviations (also in BeatmapSetList.h)
- **Node lifetime.** Java nodes live while anything references them. Here the list owns the base nodes forever (until the list dies) and the per-beatmap nodes created by `expand` until `RETIRED_EXPANSIONS` (16) newer expansions have
  happened; a raw `prev` / `next` / selected-node pointer to an expanded node older than that dangles. The song menu only keeps the current and the previous one. If you ever hold older ones, switch the nodes to `shared_ptr`.
- `addSongGroup(beatmaps, current)` takes the mods (the BeatmapSet sort needs them). **Java quirk kept:** the set ID is read with `beatmaps.get(0)` AFTER the `BeatmapSet` constructor sorted that same list in place, so it is the easiest
  difficulty's ID (they are normally all the same). An empty vector throws `std::out_of_range` (Java: IndexOutOfBounds).
- `getNode(node, shift)` with `shift <= 0` never moves (Java loop `i < shift`): the backward direction is dead code in Java, kept.
- `init()` clears the expansion without re-linking (Java does too); the expanded nodes are retired, the base nodes are re-linked from scratch.
- `expand` of a set with no beatmaps returns null but marks the index as expanded (as Java).
- **Design note for the hash table** (next slice): Java's `HashMap<String, Beatmap>` stores Beatmap references. Here a `Beatmap` is a value inside `BeatmapSet`, and `sortByDifficulty` moves them, so a raw `Beatmap*` would go stale.
  Store `(BeatmapSet*/node, file path)` and look the beatmap up by path (or make `BeatmapSet` hold `shared_ptr<Beatmap>`); decide when porting `getBeatmapFromHash`.

**Correction (found in batch 33):** the "here `nodes` and `groupNodes` are one list" shortcut above was wrong for the group ALL. In Java `BeatmapGroup.ALL.filter(list)` returns `list` itself, so `parsedNodes`,
`groupNodes` and `nodes` are ONE list: a node from `addSongGroup` is in `size()` at once, and `init()` (the real load order is `addSongGroup`... then `init()`) sorts it. Batch 32 copied the list in `reset()`, so
`size()` stayed 0 until `reset()` and an `init()` right after the adds did nothing. Fixed in batch 33 (the batch 32 test now expects `size() == 4`). The "unreachable guards" survivors above are no longer
unreachable: `search` can shrink `nodes` below `expandedIndex`; that guard is now covered by a test (see batch 33).

## Batch 33 (done, small): BeatmapSetList search slice (`search`, `applySearch`, `SearchSnapshot`, `snapshotForSearch`, `isCurrentQuery`) + the list-aliasing fix
Ported: in beatmap/BeatmapSetList.h/.cpp: `search(query)` (null -> false, normalise, same query -> false, empty query -> `nodes = groupNodes`, else `nodes` = new filtered list), `isCurrentQuery`, `snapshotForSearch`,
the `SearchSnapshot` class (copy of `groupNodes` + the owner list + its size) and `applySearch(query, snapshot, result)` (false and no change when `groupNodes` is another list or its size changed). `reset()` now clears `lastQuery`
(it did not in batch 32). The three Java list references are now `shared_ptr<vector<BeatmapSetNode*>>` (`parsedNodes_`, `groupNodes_`, `nodes_`; `ownedNodes_` owns the base nodes), so the Java identity rules hold:
ALL group = one list; FAVORITE / RECENT = new `groupNodes`; empty search = `nodes` is `groupNodes` again; `init()` sorts / links only `nodes` (after a search only the result list: **Java quirk kept**, `groupNodes` keeps its order
and the nodes outside the result are not re-linked). `SearchSnapshot` holds a `shared_ptr` to its owner, so a freed-and-recycled address can never pass the identity check (Java's reference does that for free).
Tests: tests/test_batch33.cpp (behavioural, no JDK here): aliasing for ALL (size at once, `init()` right after the adds, `reset` keeps the instance) and FAVORITE (new list, a later add reaches `parsedNodes` only,
search stays inside the group), `search` (null, normalisation, same query, source order, text term + condition, no match / empty list, empty query restores the SAME list object, `reset` forgets the query),
`init()` after a search (only the result list is sorted / linked), the expansion is untouched by `search` and `unexpand`'s `expandedIndex >= size()` guard is now hit, snapshot / `applySearch` (FAVORITE: stale after `reset`, ALL: valid
after `reset` but stale after an add, copy semantics, a snapshot outliving its list). Clean under AddressSanitizer + UBSan; batches 2-32 still pass. Mutation-checked: 17 deliberate bugs; 16 caught first time, 1 gap
(search filtering `parsedNodes` instead of `groupNodes` was invisible with the group ALL) got a test in the FAVORITE group. Not golden (no JDK).
Seams / deviations: `search` takes `std::optional<std::string>` (nullopt = Java null); `applySearch` takes the result by value and must not be given a cancelled (nullopt) filter result (Java would store null). No locking: like
Java, `search` / `applySearch` / `snapshotForSearch` are main-thread only; only `filterNodes` on `snapshot.source()` may run on another thread.

## Batch 34 (done, tiny): BeatmapSetList MD5 hash table + `getBeatmapFromHash`
Ported: in beatmap/BeatmapSetList.h/.cpp: the `beatmapHashDB` table filled by `addSongGroup` (every beatmap with a non-empty `md5Hash`; a later beatmap with the same hash overwrites, like `HashMap.put`; iterated after the
BeatmapSet sort, like Java) and `getBeatmapFromHash(hash)` (nullptr = Java null; "" is never stored, so it is a miss like `get(null)`).
Design (the note from batch 32, now decided): the table stores `(shared_ptr<BeatmapSet>, file path)` and the lookup finds the beatmap by path inside the set, so it stays correct after `BeatmapSet::sortByDifficulty` moves the
`Beatmap` values (a raw `Beatmap*` would have gone stale). The result is a `const Beatmap*` INTO the set: valid until that set is re-sorted or loses a beatmap, so callers use it at once and do not keep it. An entry whose beatmap
is no longer in its set is a miss (nullptr), not a crash.
Tests: tests/test_batch34.cpp (behavioural, no JDK here): lookup of every stored hash (pointer into the set), empty list / "" / unknown / case-sensitive misses, no hash for a beatmap without one, duplicate hash across sets and
inside one set (after the sort), lookup after `sortByDifficulty` reordered the set, counts untouched, removed beatmap = miss. Clean under AddressSanitizer + UBSan; batches 2-33 still pass. Mutation-checked: 5 deliberate bugs, all caught first time.
Not golden (no JDK).
**Left for the delete slice (35):** `deleteSongGroup` / `deleteSong` must erase the hash entries like Java (`beatmapHashDB.remove(bm.md5Hash)` by KEY: if two sets shared a hash, deleting either removes the entry for both; keep that).

## Batch 35 (done, small, BeatmapSetList PARTIAL + new `DeleteHooks` seam): `deleteSongGroup` / `deleteSong`
> **WIP:** the list logic is complete, but nothing is deleted from disk yet. The side effects go through `BeatmapSetList::DeleteHooks`, and with the default (empty) hooks the lists change while the files stay. The real
> implementations behind the hooks (BeatmapDB, MusicController, BeatmapWatchService, `Utils.deleteToTrash`) do not exist in C++ yet. Do not call these from the app before all hooks are wired.
> **To do next (for this change):** (1) write the 5 real hook backends when their classes are ported (see the WIP table row "DeleteHooks backends"); (2) the Java `create` / `destroy` / `get` singleton, the last open member of the class;
> (3) decide whether to fix the two Java quirks listed under "Java notes" below (not done here: they change behaviour).
Ported: in beatmap/BeatmapSetList.h/.cpp: `deleteSongGroup(node)`, `deleteSong(node)`, the `DeleteHooks` struct + `setDeleteHooks`, a `beatmapHashCount()` diagnostic, and java.io.File path helpers (`getName`, `getParent`, path normalisation:
duplicate / trailing `/` removed, `/x` -> parent `/`, `/` and a bare `x` have no parent) used to turn a beatmap path into the `(dirName, fileName)` the BeatmapDB wants. Statement order is Java's: re-link the neighbouring base nodes around
the expanded group, remove the node from `nodes` (by index), `parsedNodes` and `groupNodes` (by value, first match; no-ops when the lists are aliased), `mapCount`, the beatmap-set-ID set and the hash table (both BY KEY), reset the
indices (and the expansion), stop the track, drop the DB entry, pause the watch service, delete to trash, resume.
Seam (`DeleteHooks`, all `std::function`, an empty one does nothing / answers false): `trackActive`, `currentAudioFilename`, `resetTrack` (= `MusicController.reset()` + `Utils.gc(true)`), `dbDeleteDir`, `dbDeleteFile`, `watchServiceActive`,
`pauseWatchService`, `resumeWatchService`, `deleteToTrash` (a `java::IOException` is reported through `Download::reportError("Could not delete song group." / "Could not delete song.", e, true)` and swallowed, the call still returns true, like
Java; any other exception propagates and the watch service is NOT resumed, because Java has no `finally`).
Deviations: Java NullPointerException -> `std::logic_error` and IndexOutOfBounds -> `std::out_of_range`, thrown at the same step as in Java (after the lists changed); the deleted node and its sets stay valid in memory (the list keeps
its nodes alive, Java's GC would too), so a pointer to a deleted node is safe to read.
Tests: tests/test_batch35.cpp (behavioural, 123 assertion lines; the JDK cannot run BeatmapSetList, it needs the whole engine, so the expected values were derived from the Java statement by statement): list sizes / indices / forward and
backward link chains after deleting the first / middle / last / only group; with a group expanded: a group before it (not adjacent / adjacent), after it (adjacent / not), the expanded group itself; the three non-aliased list layouts
(search active, FAVORITE group, both); `deleteSong` (not expanded / base node / null -> false, middle / first / last beatmap, last beatmap deletes the group, repeated deletes); removal by key; every hook (order, arguments, watch service,
track reset only when the audio file names match and are non-empty, IOException reported / other exception propagates); empty hooks; NPE / IOOBE cases; the path rules. Clean under AddressSanitizer + UBSan over every source; batches
2-34 still pass (41 of 41). Mutation-checked: about 45 deliberate bugs in the delete code and the path helpers. The first round left 5 survivors and 3 mutants that did not apply; I added tests for the real gaps they showed ( `groupNodes` removal needs a non-ALL group AND a search,
the "both audio names empty" track case, the `deleteSong` error flag, the hash count, the NPE guards for an expanded empty set) and re-ran them all: every mutant that can change behaviour is now caught. The 2 that remain are behaviourally equivalent: `MSIDdb.remove` for an ID <= 0 (never stored, so a no-op), and starting
the `beatmapIndex` shift loop one earlier (the extra step is stopped by the `index ==` test).

Java notes (not C++ tasks; the C++ keeps the Java behaviour until the Java is changed)
- `deleteSong` does not update `expandedStartNode` / `expandedEndNode` when the deleted node was the first / last of the expansion. `unexpand` does not care (it re-links from the base nodes), but a later `deleteSongGroup` of a group BEFORE
  it walks `size` nodes starting from the stale start node, so the LAST expanded node keeps its old index (pinned in the test, "quirk"). Fix in Java: after the re-link, `if (node == expandedStartNode) expandedStartNode = node.next;` and
  the same for the end node with `prev`.
- `MSIDdb.remove(id)` and `beatmapHashDB.remove(hash)` go by KEY: if two sets share a beatmap-set ID or an MD5 hash, deleting either one removes the entry for both (pinned in the test).
- A set with no beatmaps that is "expanded" (`expand` returns null but marks the index) makes `deleteSongGroup` throw a NullPointerException (`expandedStartNode` is null); only reachable if a set was emptied behind the list's back.

## Batch 36 (done, tiny, DeleteHooks backends PARTIAL: 1 of 9 real): `Utils.deleteToTrash` / `Utils.deleteDirectory`
> **WIP:** `DeleteHooks::deleteToTrash` now has a real backend, but the other 8 hooks still have none (`trackActive`, `currentAudioFilename`, `resetTrack`, `dbDeleteDir`, `dbDeleteFile`, `watchServiceActive`, `pauseWatchService`,
> `resumeWatchService`: MusicController, BeatmapDB, BeatmapWatchService, `Utils.gc`). With only this one wired, `deleteSong*` really deletes the files but does NOT stop the track, drop the DB entry or pause the watch service. Do not call
> `deleteSong*` from the app until all 9 are wired.
> **To do next (for this change):** (1) BeatmapDB `delete(dir)` / `delete(dir, file)` seam (next in the list below); (2) decide whether to fix the Java symlink quirk below (not done: it changes behaviour); (3) unify the two copies of the
> `java.io.File` path normalisation (`normalizePath` in BeatmapSetList.cpp, `normalizeFilePath` in Utils.cpp) into one `java::` helper (small refactor, no behaviour change); (4) a Windows path rule (`\\` and drive letters) if the port ever targets Windows,
> the normalisation here is Unix-only like the rest of the File helpers.
Ported: in Utils.h/.cpp: `Utils::deleteDirectory(path)` and `Utils::deleteToTrash(path)` (returns false = "deleted, not moved to trash": the Java trash code is commented out, so it always deletes). `deleteToTrash` throws
`java::IOException("File '<absolute path>' does not exist.")` for a missing path (a broken symlink counts as missing, like `File.exists()`; the absolute path is the working directory + the path with duplicate / trailing `/` removed, as
`File.getAbsolutePath()`; the empty string gives the working directory); every other failure (permissions, a non-empty directory that cannot be emptied) is silent, like `File.delete()`. The hook backend is one line:
`hooks.deleteToTrash = [](const std::string& p) { Utils::deleteToTrash(p); };`.
Tests: **tests/test_golden10.cpp** vs **tests/golden/expected10.txt**, a transcript the real JDK 21 produced by running the Java `deleteToTrash` / `deleteDirectory` (copied VERBATIM into tests/golden/delete/DeleteRunner.java) through `java.io.File` on a real
file tree: 22 scenarios, each on a fresh fixture (tests/golden/delete/fixture.sh, scenarios.txt; regenerate with tests/golden/delete/gen10.sh, needs a POSIX sh + JDK 21): file, empty dir, dir with only a dotfile, nested tree, trailing slash, double slashes,
nested subdir, nonexistent (plain / nested / `./` / trailing slash / duplicate slashes), empty string, broken symlink, symlink to a file, symlink to a directory, a directory containing symlinks, absolute path (existing and not), unicode and space names, `f.txt/`
(a file named with a trailing slash). Each scenario prints the result or the exception message and the remaining tree. **All 22 match.** tests/test_batch36.cpp (behavioural, 27 assertions on real temporary files): the same rules, `deleteDirectory` ignoring a file /
a missing path / "", and an **integration test**: real files deleted through `BeatmapSetList::deleteSong` / `deleteSongGroup` with the real backend (one `.osu` removed, then a whole directory with a subfolder, then the already-missing directory: the IOException is
reported as "Could not delete song group." and swallowed, the list still shrinks). Clean under AddressSanitizer + UBSan; the whole suite re-run: 43 of 43 pass (41 earlier + these 2).
Found by the golden and fixed: Java's `File` normalises the path before touching the disk, so `f.txt/` is the file `f.txt`; passing the raw string to libc answers ENOTDIR (not found). Both functions now normalise first.
Mutation-checked: 11 deliberate bugs (no trailing-slash / duplicate-slash normalisation, symlinks not followed, no recursion, the directory itself kept, returns true, `exists` not following links, empty path allowed, wrong path in the message, `deleteDirectory`
deleting files). 10 caught on the first run; the duplicate-slash one survived because no scenario printed such a path, so I added "nonexistent: `nope//x//`" and re-ran: caught. The golden does not cover a read-only / unreadable directory (the sandbox runs as root, which
ignores permissions); the silent-failure rule for those is by reading the Java code, not by test.

Java notes (not C++ tasks; the C++ keeps the Java behaviour until the Java is changed)
- **`Utils.deleteDirectory` follows symbolic links into other directories.** `File.isDirectory()` follows links, so deleting a beatmap folder that contains a symlink to a directory deletes the CONTENTS OF THE LINK'S TARGET first, then the link (the JDK transcript pins this:
  "symlink to directory" and "dir containing symlinks": `ext/` is emptied). A beatmap folder with a link to e.g. a music library would wipe that library. Fix in Java: skip links, `if (java.nio.file.Files.isSymbolicLink(file.toPath())) { file.delete(); continue; }`
  before the `isDirectory()` test (and the same at the top of `deleteToTrash`). The C++ keeps the Java behaviour so the two stay comparable; say the word and both get the fix together.
- The trash move in `deleteToTrash` is commented out in Java ("TODO move to system trash"); nothing here moves to a trash either.

## Batch 37 (done, tiny, BeatmapDB PARTIAL: only the two `delete` methods): the BeatmapDB seam
> **WIP:** `db/BeatmapDB` is ONE SLICE of the 27 KB Java class: `delete(dir)` / `delete(dir, file)` and the `cacheSize` / `updateCacheSize` bookkeeping they use, over a small `BeatmapDBStore` interface (3 methods). There is **no real store**: no SQLite,
> so nothing is persisted yet. Not ported from BeatmapDB: `init` / `createDatabase` / `updateDatabase` / `getCacheSize` (the DB open + schema), `insert` (single + batch), `load` (both), `setStars`, `updatePlayStatistics`, `updateFavoriteStatus`,
> `updateLocalOffset`, `getLastModifiedMap`, `clearDatabase`, `closeConnection`, `LastModifiedMapEntry`.
> **To do next (for this change):** (1) a real `BeatmapDBStore` over SQLite (the DB-layer decision: the "Backend decision" section names no DB; sqlite3 amalgamation is the obvious pick, the Java SQL is in `BeatmapDB.java` lines 54-90 + 184-185 + the `updateSizeStmt`);
> (2) wire the two hooks (2 lines, shown in `BeatmapDB.h`); (3) then the rest of the class, smallest first: `setStars` / `updateFavoriteStatus` / `updatePlayStatistics` / `updateLocalOffset` (they only bind fields), then `insert`, `load`, `getLastModifiedMap`, `init`.
Ported: db/BeatmapDB.h/.cpp: `BeatmapDB` (instance over a non-owning `BeatmapDBStore*`, null = Java `connection == null`: both deletes return at once), `deleteMap(dir, file)` / `deleteGroup(dir)` (Java `delete` overloads; `delete` is a C++ keyword), `cacheSize()` / `setCacheSize()`
(starts at -1 like Java; `init()` would load it), `SQLException`. Behaviour kept: `cacheSize -= rows` wraps like a Java int; if the delete throws, `cacheSize` is unchanged and no size is written; the size written is `Math.max(cacheSize, 0)` as a string (the field itself may go negative);
a failing size update is reported by its OWN message ("Could not update beatmap cache size.") and not by the delete's catch; only `SQLException` is caught; reports go through `Download::reportError` (= `ErrorHandler.error(msg, e, true)`).
Tests: tests/test_batch37.cpp (behavioural, fake store, 40 assertions): no connection, rows subtracted, clamp below zero, the default -1, int wrap, failing delete, failing size update, non-SQL exceptions propagate, and an **integration test** through the real `BeatmapSetList::deleteSong` /
`deleteSongGroup` with the two DB hooks wired: the store sees the DIRECTORY NAME and file name (`map:dirA|A1.osu`, then `group:dirA`) and the size goes 10 -> 9 -> 8. Clean under AddressSanitizer + UBSan.
Mutation-checked: 9 deliberate bugs. 7 caught on the first run; "null-store check removed" crashes the test run (a segfault: caught, but not by a FAIL line); "plain int subtract" is invisible without a sanitizer (x86 wraps silently) and is caught by UBSan ("signed integer overflow", INT_MIN - 1).
NOT golden-tested: there is no SQL to run against (the logic is 3 lines per method; the SQL strings live in the future SQLite store).

## Batch 38 (done, tiny, BeatmapDB STILL PARTIAL): four single-row updates + node-typed `BeatmapGroup::filter`
> **WIP:** `db/BeatmapDB` is now TWO slices of the Java class: the two `delete` methods (batch 37) and `setStars` / `updatePlayStatistics` / `updateFavoriteStatus` / `updateLocalOffset` (this batch), all over the `BeatmapDBStore` seam (now 7 methods).
> There is still **no real store** (no SQLite), so nothing is persisted. Still not ported: `init` / `createDatabase` / `updateDatabase` / `getCacheSize`, `insert` (single + batch), `load` (both), `getLastModifiedMap`, `clearDatabase`, `closeConnection`, `LastModifiedMapEntry`.
> **To do next (for this change):** (1) the real SQLite `BeatmapDBStore` (the 4 new statements are in `BeatmapDB.java` lines 184-190 area: `UPDATE beatmaps SET stars/favorite/localOffset/playCount,lastPlayed ... WHERE dir = ? AND file = ?`);
> (2) wire the favourite hook: `BeatmapSet`'s favourite seam -> `db.updateFavoriteStatus(beatmap, useUnicode)`; (3) then `insert`, `load`, `getLastModifiedMap`, `init`, `clearDatabase`, `closeConnection`.
Ported: `BeatmapDB::setStars / updatePlayStatistics / updateFavoriteStatus / updateLocalOffset(const Beatmap&, bool useUnicode = false)`. No connection (null store) returns at once, before the file path is looked at. `dir` / `file` are `getFile().getParentFile().getName()` / `getName()` with `java.io.File`
normalisation (duplicate and trailing `/` ignored; parent of `/f.osu` is `/`, whose name is `""`). A bare file name has no parent: Java throws NullPointerException (not an SQLException, so it propagates); the C++ throws `std::logic_error` and stores nothing. A failing statement is reported with Java's exact
message through `Download::reportError`; `setStars` formats the rating with `%.4f` HALF_UP (`java::formatFixed`: 1.00005 -> "1.0001", where printf gives "1.0000"). `beatmap.toString()` in those messages reads `Options.useUnicodeMetadata` in Java; here it is the `useUnicode` argument (seam).
Also: `BeatmapGroup` got `filter(g, const std::vector<BeatmapSetNode*>&)` for the real node type (the generic template from batch 19 is unchanged and still works for any node type); this closes the "node-typed filter" item of the BeatmapGroup WIP row. The tab sprite / `contains` / `draw` are still WIP.
Tests: tests/test_batch38.cpp (behavioural, 41 assertions incl. the integration of RECENT / FAVORITE / ALL over real `BeatmapSetNode`s, the heap order of the RECENT result, the top-20 cut). Clean under AddressSanitizer + UBSan; the whole suite re-run: 45 of 45 pass.
Mutation-checked: 7 deliberate bugs (3 -> 4 digit rounding, inverted favourite, swapped play count / time, swapped dir / file, no trailing-slash strip, no-parent check removed, a typo in a message): all 7 caught on the first run.
NOT golden-tested: there is no SQL to run; the only float / locale piece (`%.4f`) is `java::formatFixed`, already JDK-compared in an earlier batch.

## Batch 39 (done, small): the OsuToPjsk converter (finishes the half-ported WIP file from batch 7) + `java::trim` / `java::splitAll`
> **WIP:** `fourresonance/OsuToPjsk` itself is COMPLETE (every method of the Java class). What is still open around it:
> (1) **nothing calls `convert` yet**: its only Java caller, `PjskGame` (18 KB game state / UI), is not ported;
> (2) callers must hand `convert` a `std::istream`: opening the chart file / Android asset into a stream is backend work, not done here;
> (3) `java::trim` now exists in `util/JavaCompat`, but **three private copies of the same byte trim remain** (`src/user/UserList.cpp`, `src/downloads/DownloadServer.cpp`, and an in-place one in `src/beatmap/BeatmapSearch.cpp`): behaviour is identical, they were left alone so no verified file was touched;
> (4) `OsuToPjsk::detail::*` is public only so the tests can reach Java's private helpers: not a stable API.
> **To do next (for this change):** (1) port `PjskGame` when the state / UI layer exists (it will call `OsuToPjsk::convert(stream, tailMode, tingedColumns, HOLD_MIN_MS, maxSimultaneous)`; check its Java for the exact arguments);
> (2) swap the three private trims for `java::trim` and re-run test_batch29-31 / 23-28 / the UserList tests; (3) the Android side: an `AAsset` / file `std::istream` for chart files.
Ported (all of `OsuToPjsk.java`): `convert` (+ `convertString` for text in memory), the tolerant `.osu` parser (`[General] Mode`, `[Difficulty] CircleSize / OverallDifficulty`, `[TimingPoints]`, `[HitObjects]`, with `BufferedReader.readLine` line ends and `String.trim` semantics),
`limitSimultaneous` (12 ms chained chord window, finger limit that keeps the notes farthest apart), the BPM segment model (backward lead-in by whole beats), 1 ms beat snapping over 1/1 .. 1/16 with the collision-restore loop, `buildTimeScale`
(timing groups, effective SV, base-scroll normalisation), `LaneLayout` (the deterministic +-1 lane drift: int-wrapping hash of the note time and column), `columnToLaneSize`, `isTingedColumn`, tail modes, `minHold`.
New shared helpers: `java::trim` (strips bytes <= 0x20, i.e. `String.trim` on UTF-8) and `java::splitAll` (`split(",", -1)`: keeps trailing empties, unlike the older `java::split`). `java::parseDouble` (batch 20) is reused.

Seams / deviations
- `java.io.Reader` -> `std::istream` (bytes; only ASCII is inspected, so UTF-8 needs no decoding). A stream that is already failed (failbit / badbit, e.g. an `ifstream` that did not open) or whose buffer throws while reading gives `java::IOException`.
  (The first version let a buffer's own exception escape and kept an unreachable post-read `bad()` check; test_batch39 found the first, mutation-checking found the second: it is deleted.)
- `null` chart -> `std::nullopt`.
- **A key count above `MAX_KEYS` (1 << 20) throws `std::length_error`.** The Java would run out of memory allocating its per-column arrays (e.g. `CircleSize:1e12`); this is the only intentional behaviour difference and it is NOT golden-testable (the JVM dies). NaN / negative / fractional circle sizes are fine and match the Java.
- `String.toLowerCase()` on the ASCII-only keys is locale independent; `Double.parseDouble` is `java::parseDouble` (no hex floats, like `parseFloat`).
- Java `int` arithmetic is reproduced where it can wrap (chord window, hold length vs `minHold`, the lane hash, the idle test, the lead-in beat count), so absurd times behave exactly like the Java instead of being undefined behaviour. Java sorts are stable: `std::stable_sort` everywhere (the golden suite catches an unstable one).
- `buildNoteBeats` iterates `std::set` / `std::map` where Java used `TreeSet` / `HashMap`; nothing depends on the unordered one's order.

Tests
- **tests/test_golden11.cpp** vs tests/golden/expected11.txt, produced by the REAL unmodified `OsuToPjsk.java` + `PjskChart.java` (no stubs needed, they only use `java.*`; JDK 21; `tests/golden/java/gen11.sh <core/src>`). New design: the Java writes an input corpus (`cases11.txt`, 0.9 MB) next to the expected output (0.7 MB) and the C++ replays it, so no random generator is mirrored.
  1209 cases / 8323 lines, all match: 216 `String.trim` / `split(",", -1)` strings, 127 groups of beat / time conversions + snapping (8-16 times each; incl. NaN / infinity, tied snap candidates at a ~1 ms grid, the exact 1 ms + 1e-6 tolerance edge +- 1 ulp), 96 LaneLayout runs (every key count 0 .. 40 and negatives, wrapping times), 190 `columnToLaneSize`, 150 `isTingedColumn`, and 430 end-to-end conversions (264 give a chart with 4,920 notes in total, 166 are rejected) from fuzzed files x all tail modes / tinged / min-hold / finger limits.
- **tests/test_batch39.cpp** (56 assertions, behavioural): what a JVM cannot compare: hand-computed charts (lanes, beats, tinged, lead-in), default arguments, `convert(istream)` == `convertString`, `IOException` paths, the `MAX_KEYS` guard and its boundary (1 << 20 keys really converts), helper edge cases.
- Clean under AddressSanitizer + UBSan (both new suites); the whole suite re-run: **47 of 47 pass**, and the first 45 results are identical to the batch-38 baseline.

Mutation check (92 deliberate bugs injected one at a time into the converter and the two helpers; a harness relinks the mutated file and runs the suite): **80 caught, 12 survived** (final pass over all 92). In the first pass over the first 47 of them, 12 had survived and about half were real test gaps; those, and three more found by the later mutants, were fixed by extending the generator and the hand-made cases:
`Mode: 4` accepted, a hold ending exactly when the next note starts (`<=` vs `<`), a hold end in field 6 without a colon, the header look-alikes `[x]y]` and `[]`, tied snap candidates, the tolerance edge, 24+ timing points at equal times (stability beyond libstdc++'s insertion-sort threshold), dense fast-tempo clusters.
The 12 survivors, all judged equivalent / unreachable (not "missed"): (a) my own broken no-op mutation (the 350 ms constant lives in the header; the properly applied 349 / 351 versions are caught); (b) `(positions - 1) / 2` vs `positions / 2` for one key (both 5); (c) lane `floor` vs truncation (the clamp to 0 absorbs it);
(d) `colon > 0` (an empty number throws a swallowed NumberFormatException anyway); (e) the `//` comment test (a commented-out line can never parse as data); (f) the key-charset check and (g) the `colon == 0` test (neither can produce a key that matches `mode` / `circlesize` / `overalldifficulty`); (h) snap division 3 (every third is also a sixth: identical double);
(i) `svMultiplier` and (j) `bpm` guards (only ever called for non-red / red points); (k) `original <= next` in the collision test (needs two different times with the identical beat); (l) **the restore loop's second pass: a search over 3,000,000 random dense inputs never found one where it changes anything** (kept as in the Java; documented here so nobody "simplifies" it by accident).
Not golden-tested: the `IOException` / `MAX_KEYS` behaviour above (C++-only), and `Double.parseDouble` hex floats (unsupported, see batch 20).

## Batch 40 (done, small): Storyboard (the data model + evaluation, complete for this Java file)
> *Merge note: this was developed on a parallel branch as "batch 38" (the other branch's batches 38 / 39 are the BeatmapDB updates and the OsuToPjsk converter above). It was merged on top of batch 39 and renumbered 40; its golden suite became **golden12** because golden11 is the OsuToPjsk suite. The only other differences between the branches were stale "PARTIAL" comments that batch 39 had already updated.*
Ported: storyboard/Storyboard.h/.cpp (16 KB Java): the whole of `Storyboard.java`: `Command`, `CommandGroup`, `LoopGroup`, `TriggerGroup`, `State`, `Sprite` (`finish`, `isAlive`, the loop-aware `pick`, `evaluate`), `Sample`, `Layer`, and `Storyboard` itself (the six built-in layers,
`getLayer` with auto depth, `hasDrawable`, `hasSamples`, `replacesBackground`, `getDrawOrder`, `finish`). It is a port of osu!lazer's `osu.Game.Storyboards` command model; the Java file is pure data + math, so nothing is partial here.
> **Not part of this batch (each still unported, nothing half-done):** `StoryboardParser` (14 KB, fills this model: the next batch), `StoryboardRenderer` (drawing: needs the graphics backend), and the `Storyboard*` UI glue in the game states.
> **To do next (for this change):** (1) `StoryboardParser` (it uses the "package-private" members that are PUBLIC here: the `Command` constructor, `Sprite` constructor, `addLoopGroup`, `addTriggerGroup`, `Layer::sprites` / `samples`, `Storyboard::finish`); it needs a text-file reading seam only;
> (2) the renderer once there is a graphics backend; (3) wire `replacesBackground` / `hasDrawable` into the game states when they exist.
Tests: **tests/test_golden12.cpp** against `tests/golden/expected12.txt` (4119 lines), generated from the REAL Java `Storyboard` + `AnimationEquation` by `tests/golden/storyboard/gen12.sh` (the runner is in the Java package so it can use the package-private API). 150 random sprites (random commands on all 10
properties, loop groups with repeat counts -2..5, trigger groups, empty sprites, equal / negative / half-ms times) each evaluated at 26 times, plus hand-made cases: `end < start` clamping, `INT_MAX` repeat wrap (`totalIterations` = `INT_MIN`), looping alpha / position, flips and blend, alpha > 1 flicker wrap, ties between loop commands,
60 equal-start commands (sort stability), layer depths / draw order / cache, `replacesBackground`. All lines match bit for bit (floats and doubles compared as hex).
Mutation-checked: 19 deliberate bugs (iteration cap, winner tie-break, binary search bound, end clamp, alpha wrap, loop end time, first-alpha rule, ended test, visibility test, both stable sorts, `max(0, repeat)`, draw order, case folding, depth decrement, loop offset, earliest time, initial value, draw-order cache).
17 caught first time; 2 survived (loop tie-break `>=` vs `>`, and `stable_sort` vs `sort`: libstdc++ only reorders ties above 16 elements), so SPEC7 / SPEC8 were added and now catch both. One more mutation (`hasLoopCommands` threshold `> 1` vs `> 0`) is EQUIVALENT (identical results for one-iteration loops), not a test gap.

Seams / deviations
- Java "package-private" members are PUBLIC (see above); `Layer` / `Sprite` / `LoopGroup` / `TriggerGroup` are owned by `unique_ptr`s, so `Command::loop` is a non-owning pointer and the classes are not copyable. `Sprite::finish()` builds the merged command lists as COPIES (Java kept references): call `finish()` after the last command is added.
- `float[] v0, v1` are `std::vector<float>` read with `.at()` (a short command throws `std::out_of_range` instead of ArrayIndexOutOfBounds). `Collections.sort` is `std::stable_sort` with `java::compare`. `LinkedHashMap` is an insertion-ordered vector + name index.
- `replacesBackground` compares ASCII case-insensitively (Java `equalsIgnoreCase` also folds non-ASCII letters). Nullable `String` is `std::optional<std::string>`. `repeatCount + 1` and `--minimumLayerDepth` wrap like Java ints. The sprite scratch buffers make `evaluate` not thread safe (same as Java).

## WIP: partially ported files (finish these when the backend exists)
| File | Done | Still missing |
|---|---|---|
| **BeatmapGroup** (batch 19) | names / ids / messages, the 3 filters (incl. the PriorityQueue order), `current` / `set`, tab layout (`init`, centre, left edge) | the tab `MenuButton` sprite, `contains`, `draw` (UI.drawTab). *(node-typed `filter` done in batch 38)* **To do next:** with the sprite backend (MenuButton + GameImage.MENU_TAB) |
| **BeatmapSetNode** (batch 31) | data, `getSelectedBeatmap`, `getInfo`, `toString`, `truncate` / `fit` / rating text | `drawSetPanel`, `drawDifficultyRow`, `drawFlash`, panel slicing / solid mask images, `drawStarPill`, `fillCircle`, `fillRounded` (Graphics, Image, Fonts, Colors, GameImage, Options.isSongThumbnailsEnabled) |
| **BeatmapSetList** (batches 30, 32, 33, 34, 35) | `normalizeQuery`, query parsing, static `filter` (BeatmapSearch, batch 30); node lists (Java aliasing, fixed in 33), `reset`, `init`, `addSongGroup`, counts, set-ID table, `getBaseNode` / `getRandomNode` / `getNode`, `expand` / `unexpand` (32); `search` / `applySearch` / `SearchSnapshot` / `snapshotForSearch` / `isCurrentQuery` (33); MD5 hash table + `getBeatmapFromHash` (34); `deleteSongGroup` / `deleteSong` over `DeleteHooks` (35) | the static `create` / `destroy` / `get` (construct the object where needed); **to do next:** the real hook backends (next row) |
| **DeleteHooks backends** (batches 35, 36, 37) | the seam (9 hooks); `deleteToTrash` has its real backend (batch 36, JDK-compared); `dbDeleteDir` / `dbDeleteFile` have their logic (batch 37: `BeatmapDB::deleteGroup` / `deleteMap` over the `BeatmapDBStore` seam, wiring shown in `BeatmapDB.h`) but NO real SQLite store | **6 of 9 not real at all**: `trackActive` / `currentAudioFilename` / `resetTrack` (`MusicController`, `Utils.gc`), `watchServiceActive` / `pauseWatchService` / `resumeWatchService` (`Options.isWatchServiceEnabled` + `BeatmapWatchService`); **and the 2 DB hooks need a real store**. **Until all are wired `deleteSong*` must not be called from the app.** **To do next:** a SQLite `BeatmapDBStore`, then MusicController |
| **BeatmapDB** (batches 37, 38) | `delete(dir)`, `delete(dir, file)`, `cacheSize` bookkeeping, `setStars`, `updatePlayStatistics`, `updateFavoriteStatus`, `updateLocalOffset` over `BeatmapDBStore` | everything else (see the batch 38 WIP note): schema / init, insert, load, last-modified map, clear, close; and a real SQLite `BeatmapDBStore`. **To do next:** SQLite store, then `insert` / `load` |
| **BeatmapSet** (batch 29) | complete (sort, info lines, both `matches`, favourite / played) over the `ModParams` / hook seams | nothing; only the real `BeatmapDB.updateFavoriteStatus` behind the hook, and non-ASCII `toLowerCase` |
| **DownloadNode** (batches 17, 18) | data half, plus (18) layout / `init`, geometry, hit tests, `getCardRect`, clip rect, `fit` / `updateFit`, icon rects, `animate()` + state getters | `drawResult` (needs Graphics, sprites, GameImage, DownloadThumbnails), `drawResultScrollbar` (UI.drawScrollbar), the `g.setClip` call, `rect` / `rgba` helpers, `ringColors` (needs Colors.getDifficultyColor) |
| **DownloadServer** (batches 23-28) | base class (parsing, lookup, JSON fetch over `HttpClient`, `reportError`) + all 4 live servers (**Catboy, OsuDirect, Hinamizawa, Nekoha**) | **nothing left in the servers**; only a real `HttpClient` (see Download). The 7 dead ones are skipped on purpose |
| Download (batch 17) | complete, but over the `HttpClient` seam | a real `HttpClient` (libcurl on desktop, NDK / JNI on Android) |
| Url (batch 17) | subset of `java.net.URL` | percent-decoding, IPv6 / userinfo validation, Java's `http:foo` relative form (only if a server needs it) |
| TouchCursor / GameplayInput (batch 16) | complete, per-finger sprite over the `CursorSprite` seam | the real `ui/Cursor` (rendering) implementing `CursorSprite` |
| Curve + subclasses (batch 2) | geometry | `draw` / `drawRange` / `splice` / `discardGeometry`, `SliderBodyRenderer` |
| GameMod, PlaybackSpeed (batch 2) | logic | images, MenuButtons, layout, draw / hover |
| Beatmap (batch 4) | data, codecs, mod stars | background image cache + draw |
| UselessUtils (batch 10) | `screenshotFileName`, `makeOpaque` | `takeScreenshot`, `savePixmapTo` (GL read-back, PNG writer) |
| OsuToPjsk (batches 7, 39) | **complete** (BPM helpers + the whole converter, golden11) | nothing in the file; no caller yet (`PjskGame` not ported), and the chart must arrive as a `std::istream` (Android asset / file opening is backend work). **To do next:** `PjskGame` |
| `java::trim` duplicates (batch 39) | shared `java::trim` / `splitAll` in `util/JavaCompat` | 3 private byte-trim copies remain: `src/user/UserList.cpp`, `src/downloads/DownloadServer.cpp`, `src/beatmap/BeatmapSearch.cpp` (in place). **To do next:** replace them and re-run their tests |
| PjskStage (batch 8) | constants | drawing, `touchToLane()` |
| BeatmapHPDropRateCalculator / BeatmapDifficultyCalculator (batches 4, 15) | calculation | loading (`BeatmapDB.load` + `BeatmapParser.parseHitObjects` are the caller's job until those are ported) |

## Coverage snapshot (batch 37, updated for batch 40; unchanged by 38 and 39: only existing files grew; `OsuToPjsk` was already counted by name, its 24 KB are now really ported): how much of the Java core is ported
Counted from the Java core zip (`core/src`, 239 files, 2.5 MB) by NAME: a Java class counts as "ported" when a C++ header of that name or a type of that name exists. Rough, not exact (a PARTIAL file counts as ported: see the WIP table for those; nested / helper classes are matched by name too).
- **Ported (name found): 72 files, 0.59 MB (24% of the bytes). Not ported: 167 files, 1.92 MB** (batch 40 added Storyboard; before it: 71 files / 0.57 MB / 168 / 1.94 MB).
- Of the 168: **7 are the dead download servers (34 KB) skipped on purpose**, 46 are `fluddokt/opsu/fake` libGDX wrappers (120 KB; many are replaced by the C++ standard library, see "Intentionally not ported") and 19 more are libGDX glue (`fake/gl` 8, `fake/gui` 5, `newdawn/slick/state/transition` 6).
  The rest is UI, rendering, audio, DB and the game states; those wait on the backend (graphics / audio / SQLite), not on more porting.
- Biggest unported, by Java package: `states` 10 files / 469 KB (SongMenu 138 KB, Game 133 KB, MainMenu 56 KB, DownloadsMenu 53 KB), `ui` 23 / 354 KB, `options` 3 / 138 KB (Options 71 KB), `user` 4 / 113 KB, `objects` 6 / 90 KB (Slider 39 KB, Spinner 23 KB),
  `render` 7 / 64 KB, `beatmap` 5 / 59 KB (BeatmapParser 31 KB), `storyboard` 2 / 42 KB, `audio` 3 / 53 KB, `db` 3 / 47 KB (BeatmapDB, ScoreDB, ...), `downloads` 6 / 100 KB, `skins` 5 / 45 KB.
- The pure-logic candidates that remain are few: `BeatmapParser`, `Options` (needs the option-value types), `StoryboardParser`, `AutoCursor`, `Slider` / `Spinner` logic halves, and the rest of `BeatmapDB` / `ScoreDB`.

## Next up (to do, in this order; one small batch at a time)
1. *(done)* The download servers are finished (batches 25-28). Their list entry was here; the only open piece on the network side is a real `HttpClient` (see item 4).
2. *(Java note, not a C++ task)* `HinamizawaServer` and `NekohaServer` call `parseItem(arr.optJSONObject(i))`, which throws an uncaught NullPointerException for a non-object array element (and, in Nekoha, for page <= 0,
   where the slice starts at a negative index). A one-line Java fix in each (`if (item == null) continue;`, and clamp `start` to >= 0 in Nekoha) would match the "skip an odd entry" comment; the C++ keeps the Java behaviour until then.
3. Then the next pure-logic files, in this order of readiness:
   - **`BeatmapSetList` (the class): finished except the singleton.** Done: BeatmapSet (29), search / filter (30), BeatmapSetNode data + text (31), list core (32), `search` / `applySearch` + aliasing fix (33), MD5 table (34), `deleteSongGroup` /
     `deleteSong` over `DeleteHooks` (35). Left: the Java `create` / `destroy` / `get` singleton (construct the object where needed) and the hook backends below.
   - *(done, batch 36)* **`Utils.deleteToTrash`** / `deleteDirectory`, JDK-compared. *(done, batch 37)* the BeatmapDB delete seam. **Next:** the rest of BeatmapDB when the SQLite choice is made (*(done, batch 38: `setStars`, `updateFavoriteStatus`, `updatePlayStatistics`, `updateLocalOffset`)* then `insert`, `load`, `init`), then the remaining hook backends (MusicController, watch service).
   - `BeatmapParser` (needs `Options` + File / MD5 seams and the beatmap DB seam),
   - `Options`,
   - *(done, batch 40)* `Storyboard`. **Next: `StoryboardParser` (14 KB, fills this model; needs only a text-file reading seam), then `AutoCursor` (16 KB, needs only `HitObject` + `Vec2f`, both ported).**
   - *(done, batch 39)* the `OsuToPjsk` converter. The PJSK package is now complete except `PjskStage` drawing and `PjskGame` (state / UI): both wait on the backend.
   - small clean-up: replace the three private byte trims with `java::trim` (see the WIP table).
4. Everything else is waiting on a decision / the backend, not on more porting: rendering / audio / state classes and the DB layer (see "Backend decision") and **a real `HttpClient`**
   (libcurl on desktop, NDK / JNI on Android), the one open choice on the network side. Everything below ~4 KB that is still unported is engine glue or rendering (fake.* wrappers, GL,
   transitions, StarFountain, KiaiFlash, DummyObject, DBController, PulseAudioFixerListener).

Rule of thumb that has worked: check each candidate against what is already ported, port a small slice, test it (behavioural, or golden against the real Java where it is float / locale
sensitive), mutation-check the tests, then update this file. Anything left half done gets a line in the WIP table above, never silently.
