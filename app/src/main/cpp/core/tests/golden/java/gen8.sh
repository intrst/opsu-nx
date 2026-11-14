#!/bin/sh
# Regenerates ../expected8.txt from the running JDK (eighth golden suite: URLEncoder.encode(s, "UTF-8")). No core sources needed.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT/cls" "$HERE/Golden8.java"
java -cp "$OUT/cls" Golden8 "$HERE/../expected8.txt"
rm -rf "$OUT"
