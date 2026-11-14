# T0b: float parsing without std::from_chars

## Done
- `core/src/util/JavaCompat.cpp` (`parseFp`): replaced the floating-point `std::from_chars` with `std::strtof` (float) / `std::strtod` (double). Added `#include <type_traits>`. Nothing else changed.
- Why: libc++ in older Android NDKs has no floating-point `from_chars` (only added in recent versions), so the Android build could fail to compile this file. Desktop g++ was fine, so it was never seen.
- Input is already validated to plain decimal text before this call, so `strtof`/`strtod` parse exactly the same grammar. Out of range gives +-inf / 0 like Java.

## Tests
- All 48 desktop tests pass (same as before, run with the same arguments as `add_test`).
- Equality stress test (3,000,009 random decimal strings incl. exponents, long fractions, denormals, float half-way cases): `from_chars` and `strtof`/`strtod` gave bit-identical results, 0 mismatches.

## WIP / not verified
- Not built with the Android NDK (no SDK/NDK in this sandbox). Run `./gradlew assembleDebug`.
- `std::to_chars` for floating point (shortest round-trip, used for Java's `Float.toString`/`Double.toString`) is unchanged. It exists in libc++ since LLVM 14, so NDK r25+ should be fine. If the NDK build complains about it, that is the next thing to look at.
- Relies on the "C" locale (the default; the core never calls `setlocale`). Do not call `setlocale` in the app.
