#!/bin/sh
# Regenerates ../expected5.txt from the REAL DownloadNode.java (fifth golden suite: card layout, hit tests, text
# fitting, download-icon geometry and animate()). Everything DownloadNode touches is a tiny stub in stubs5/;
# drawResult is compiled but never run. Private members are called by reflection.
# usage: gen5.sh /path/to/core/src   (the directory containing itdelatrisu/)
set -e
SRC="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
# the stub constant must be the real one
grep -q 'public static final float ASPECT = 0.9f;' "$SRC/itdelatrisu/opsu/downloads/DownloadThumbnails.java" || { echo "DownloadThumbnails.ASPECT changed: update stubs5 and DownloadNode::THUMB_ASPECT"; exit 1; }
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT/cls" \
  $(find "$HERE/stubs5" "$HERE/stubs3/com" -name '*.java') "$HERE/Golden5.java" \
  "$SRC/fluddokt/opsu/fake/Color.java" "$SRC/fluddokt/opsu/fake/Integer2.java" "$SRC/itdelatrisu/opsu/downloads/DownloadNode.java"
timeout 600 java -cp "$OUT/cls" Golden5 "$HERE/../expected5.txt"
rm -rf "$OUT"
