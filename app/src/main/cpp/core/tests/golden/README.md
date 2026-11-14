# Golden tests against the real Java

`expected.txt` is produced by compiling the **unmodified** Java classes (`HitObject`, `TimingPoint`,
`GameMod`, `ReplayFrame`, all of `objects/curves/*`) with tiny stubs for their engine/UI
dependencies (`java/stubs/`) and running `java/Golden.java`. `../test_golden.cpp` regenerates the
same output with the C++ port and compares line by line. Floats are compared as raw IEEE bits.

Covers: Java number formatting/parsing semantics (`%.2f` HALF_UP, `%g`, `Float.toString`,
`DecimalFormat`, `String.split`, `parseInt/Float`), timing points, hit-object parsing/`toString`/scaling
(stacks, HardRock, several resolutions and playfield scales), every slider curve type, replay frames,
and the GameMod toggle/exclusivity/state logic.

Regenerate after changing the Java (needs a JDK 17+; `javac` is invoked through the jdk.compiler module):

    tests/golden/java/gen.sh /path/to/core/src      # the dir that contains itdelatrisu/

Not verifiable this way: libGDX key codes in `input/Keys.h` (the stub `Input` just repeats the
same numbers) and Math.pow/atan2/cos last-ulp differences between JVM and libm on other platforms.

## Third suite: Beatmap + BeatmapHPDropRateCalculator
`expected3.txt` comes from `java/Golden3.java` running the real `Beatmap`, `BeatmapHPDropRateCalculator`, `Health`,
`HitObject`, `TimingPoint`, `GameMod`, curves, `fake.Color`, `Integer2` and `LRUCache` (engine edges stubbed in
`java/stubs3`; the `Utils`/`GameData` stubs are extracted from the real files). `../test_golden3.cpp` mirrors it.
Regenerate: `tests/golden/java/gen3.sh /path/to/core/src`.
Covers: colour truncation, every DB string codec (incl. error/partial-state behaviour), `compareTo` (NaN, -0.0, int
wrap), text/unicode accessors, mod-parameter keys and the mod-star cache (incl. stale results), and the HP drop rate on
random maps, slider-heavy maps (covers the combo-end branch) and the failure orders.

## Fourth suite: BeatmapDifficultyCalculator + PerformanceCalculator
`expected4.txt` comes from `java/Golden4.java` running the real `BeatmapDifficultyCalculator` and `PerformanceCalculator` (on the real
`Beatmap`, `HitObject`, `TimingPoint`, curves; DB/parser edges are the `stubs3` ones). `../test_golden4.cpp` mirrors it.
Regenerate: `tests/golden/java/gen4.sh /path/to/core/src`. The Java is run with default HotSpot (x86-64, JDK 21).
Compared bit-for-bit except double results that go through `pow`, which may differ by 16 ulp (libm rounding differs between JVMs/libms,
see PORTING_NOTES.md batch 15); `opsu_test_golden4 expected4.txt --strict` demands exactness (1 line differs on glibc: 2 ulp).
Covers: 6 cases before any `HitObject.init`, 180 random maps x (default + random circle size / clock rate), the failure orders
(unloaded, empty, no timing points, equal times, slider without control points, zero / negative slider length), timing points exactly at /
just after an object's time, `getStars`/`getDifficulty` bounds, 160 random scores (mods, hit counts, zero hits, int overflow) and
`ratio()` corner cases (NaN, infinity, negative max).

`expected5.txt` comes from `java/Golden5.java` running the real `DownloadNode.java` (card layout, hit tests, text fitting, download-icon geometry,
`animate()`, `updateFit`) against the tiny `stubs5` edges; private members are called by reflection. `../test_golden5.cpp` mirrors it.
Regenerate with `java/gen5.sh <core/src>`.

## Sixth suite: BeatmapGroup
`expected6.txt` comes from `java/Golden6.java` running the real `BeatmapGroup` and `Long2` (stubs in `java/stubs6`). `../test_golden6.cpp` mirrors it.
Regenerate: `tests/golden/java/gen6.sh /path/to/core/src`.
Covers: the three group filters on random libraries (incl. the PriorityQueue iteration order, ties, and the `Long2.compare` 32-bit overflow quirk) and the tab layout
arithmetic (both spacing branches and the exact `tabOffset == tabWidth` boundary).

## Seventh suite: Double.toString / Double.parseDouble
`expected7.txt` comes from `java/Golden7.java` running on the JDK itself (no core sources). It is one-sided: the file holds the inputs (bit patterns / strings) and the JDK's
answers, and `../test_golden7.cpp` checks util/JavaCompat's `doubleToString` / `parseDouble` against them. Regenerate: `tests/golden/java/gen7.sh` (JDK 19+).

## Eleventh suite: OsuToPjsk (the osu!mania -> PJSK converter)
`expected11.txt` + `cases11.txt` come from `java/Golden11.java` running the UNMODIFIED `fourresonance/OsuToPjsk.java` and `PjskChart.java` (no stubs: they only
use `java.*`). Unlike the earlier suites the C++ side mirrors no generator: `cases11.txt` is the input corpus (one case per line, text as hex), written by the Java
next to `expected11.txt`, and `../test_golden11.cpp` replays it and diffs the output. Regenerate both: `tests/golden/java/gen11.sh /path/to/core/src` (JDK 21).
Covers: `String.trim` / `split(",", -1)` (the new `java::trim` / `splitAll`); `timeMsToBeat` / `beatToTime` and the private `noteTimeMsToBeat` (by reflection) incl. NaN / infinity,
tempo so fast that snap candidates tie, and the exact 1 ms + 1e-6 tolerance edge; `LaneLayout` (all key counts incl. 0 and negatives, int-wrapping times, the 349 / 350 / 351 ms
idle boundary); `columnToLaneSize`, `isTingedColumn`; and ~270 end-to-end charts from fuzzed `.osu` files (mixed line ends, whitespace, bad numbers, unsorted / duplicate
timing points and objects, chords inside / outside the 12 ms window, holds ending exactly at the next note, malformed hold ends, header look-alikes, dense fast-tempo
clusters, extreme ints), x every tail mode / tinged flag / min-hold / finger limit. The harness was mutation-checked (92 bugs, 80 caught; see PORTING_NOTES.md batch 39).

## Twelfth suite: Storyboard (`expected12.txt`, `storyboard/`)
`storyboard/gen12.sh <core/src>` compiles the REAL `Storyboard.java` + `AnimationEquation.java` with `StoryboardGolden.java` (same Java package, so it can use the package-private API the parser uses) and writes `expected12.txt` (4119 lines: 150 random sprites x 26 evaluation times, plus hand-made loop / alpha / flip / layer cases).
`tests/test_golden12.cpp` mirrors the generator statement for statement and diffs against it; floats and doubles are compared as hex bits. Run: `opsu_test_golden12 tests/golden/expected12.txt`.
(Developed as "golden 11" on a parallel branch; renamed to 12 when merged after the OsuToPjsk suite, which kept the number 11.)

