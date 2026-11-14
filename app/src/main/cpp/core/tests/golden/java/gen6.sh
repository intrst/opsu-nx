#!/bin/sh
# Regenerates ../expected6.txt from the REAL BeatmapGroup.java (sixth golden suite: group filters incl. the PriorityQueue
# iteration order and the Long2.compare overflow, plus the tab layout arithmetic). BeatmapSet / BeatmapSetNode / Beatmap /
# MenuButton / UI / GameImage / Image are tiny stubs in stubs6/; Long2 is the real one.
# usage: gen6.sh /path/to/core/src   (the directory containing itdelatrisu/)
set -e
SRC="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT/cls" \
  $(find "$HERE/stubs6" -name '*.java') "$HERE/Golden6.java" \
  "$SRC/fluddokt/opsu/fake/Long2.java" "$SRC/itdelatrisu/opsu/beatmap/BeatmapGroup.java"
timeout 600 java -cp "$OUT/cls" Golden6 "$HERE/../expected6.txt"
rm -rf "$OUT"
