#!/bin/sh
# Regenerates ../expected11.txt and ../cases11.txt from the REAL Java sources: the unmodified fourresonance/OsuToPjsk.java
# and PjskChart.java (no stubs needed: they only use java.*). cases11.txt is the input corpus (one case per line, text as
# hex); test_golden11.cpp replays it and compares with expected11.txt.
# usage: gen11.sh /path/to/core/src   (the directory containing fourresonance/)
set -e
SRC="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT" "$HERE/Golden11.java" "$SRC/fourresonance/OsuToPjsk.java" "$SRC/fourresonance/PjskChart.java"
timeout 600 java -cp "$OUT" fourresonace.Golden11 "$HERE/../expected11.txt" "$HERE/../cases11.txt"
rm -rf "$OUT"
