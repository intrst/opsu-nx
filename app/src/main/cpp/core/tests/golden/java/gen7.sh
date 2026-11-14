#!/bin/sh
# Regenerates ../expected7.txt from the running JDK (seventh golden suite: Double.toString / Double.parseDouble).
# Needs JDK 19+ for the shortest-representation Double.toString. No core sources needed.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT/cls" "$HERE/Golden7.java"
timeout 600 java -cp "$OUT/cls" Golden7 "$HERE/../expected7.txt"
rm -rf "$OUT"
