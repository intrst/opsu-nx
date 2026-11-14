#!/bin/sh
# Regenerates ../expected.txt from the real Java sources.
# usage: gen.sh /path/to/core/src   (the directory containing itdelatrisu/)
set -e
SRC="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
O="$SRC/itdelatrisu/opsu"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT" \
  $(find "$HERE/stubs" -name '*.java') "$HERE/Golden.java" \
  "$O/GameMod.java" "$O/beatmap/HitObject.java" "$O/beatmap/TimingPoint.java" "$O/replay/ReplayFrame.java" \
  "$O"/objects/curves/*.java
java -cp "$OUT" Golden "$HERE/../expected.txt"
rm -rf "$OUT"
