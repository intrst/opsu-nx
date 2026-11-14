#!/bin/sh
# Regenerates ../expected9.txt from the running JDK (ninth golden suite: SimpleDateFormat as the download servers use it, per time zone). No core sources needed.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT/cls" "$HERE/Golden9.java"
java -Duser.language=en -Duser.country=US -cp "$OUT/cls" Golden9 "$HERE/../expected9.txt"
rm -rf "$OUT"
