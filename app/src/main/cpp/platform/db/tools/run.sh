#!/usr/bin/env bash
# Desktop build + run of the T5 platform tests (the platform code is not part of the core CMake tests).
#
#   platform/db/tools/run.sh <test> [flavor]
#     test   : wrapper | beatmapdb | all
#     flavor : plain (default) | asan (ASan+UBSan+LSan) | tsan
#   env: BUILD_DIR (default /tmp/opsu_t5_build)   CC / CXX   JOBS   OPSU_REAL_DB=/path/to/real/opsu.db (optional, test 7)
#
# sqlite3.c is C: it is compiled by gcc on its own and never #included. The core sources are compiled once per flavor and cached.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CPP="$(cd "$HERE/../../.." && pwd)"          # app/src/main/cpp
CORE="$CPP/core"; SQLITE="$CPP/third_party/sqlite"
BUILD="${BUILD_DIR:-/tmp/opsu_t5_build}"
CC="${CC:-gcc}"; CXX="${CXX:-g++}"; JOBS="${JOBS:-$(nproc)}"
WHAT="${1:-all}"; FLAVOR="${2:-plain}"

case "$FLAVOR" in
  plain) SAN="" ;;
  asan)  SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer" ;;
  tsan)  SAN="-fsanitize=thread" ;;
  *) echo "unknown flavor $FLAVOR"; exit 2 ;;
esac
OUT="$BUILD/$FLAVOR"; mkdir -p "$OUT/core" "$OUT/bin"
CXXFLAGS="-std=c++17 -O1 -g -ffp-contract=off -Wall -Wextra $SAN"
INC="-I$CORE/include -I$CPP/platform/db -I$SQLITE"

# --- SQLite (C, once per flavor; same defines the integrator CMake target uses) ---
SQLITE_DEFS="-DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_MEMSTATUS=0"
if [ ! -f "$OUT/sqlite3.o" ] || [ "$SQLITE/sqlite3.c" -nt "$OUT/sqlite3.o" ]; then
  echo "[build] sqlite3.c ($FLAVOR)"
  $CC -O1 -g $SQLITE_DEFS -c "$SQLITE/sqlite3.c" -o "$OUT/sqlite3.o"
fi

# --- core library (only tests that need it) ---
build_core() {
  local objs=()
  mapfile -t srcs < <(cd "$CORE/src" && find . -name '*.cpp' | sort)
  printf '%s\n' "${srcs[@]}" | xargs -P "$JOBS" -I{} bash -c '
    s="$1"; o="$2/core/$(echo "${s#./}" | tr / _).o"
    if [ ! -f "$o" ] || [ "$3/src/$s" -nt "$o" ] || [ -n "$(find "$3/include" -newer "$o" -name "*.h" -print -quit)" ]; then
      '"$CXX $CXXFLAGS -I$CORE/include"' -c "$3/src/$s" -o "$o"
    fi' _ {} "$OUT" "$CORE"
}

link_test() {  # name needs_core
  local name="$1" needs_core="$2" objs=("$OUT/sqlite3.o" "$OUT/platform_sqlite.o")
  $CXX $CXXFLAGS $INC -c "$CPP/platform/db/SqliteConnection.cpp" -o "$OUT/platform_sqlite.o"
  if [ "$needs_core" = 1 ]; then build_core; objs+=("$OUT"/core/*.o); fi
  $CXX $CXXFLAGS $INC "$HERE/$name.cpp" "${objs[@]}" -lpthread -o "$OUT/bin/$name"
}

run_test() {
  echo "=== $1 ($FLAVOR) ==="
  ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1 TSAN_OPTIONS=halt_on_error=1 "$OUT/bin/$1"
}

case "$WHAT" in
  wrapper)   link_test test_sqlite_wrapper 0;  run_test test_sqlite_wrapper ;;
  beatmapdb) link_test test_beatmapdb_sqlite 1; run_test test_beatmapdb_sqlite ;;
  all)       link_test test_sqlite_wrapper 0;  run_test test_sqlite_wrapper
             if [ -f "$HERE/test_beatmapdb_sqlite.cpp" ]; then link_test test_beatmapdb_sqlite 1; run_test test_beatmapdb_sqlite; fi ;;
  *) echo "unknown test $WHAT"; exit 2 ;;
esac
