#!/bin/sh
# Regenerates ../expected4.txt from the REAL Java sources (fourth golden suite: BeatmapDifficultyCalculator and PerformanceCalculator (on top of the real Beatmap,
# HitObject and curves; DB/parser edges stubbed in stubs3). The Utils/GameData stubs are
# extracted from the real files at generation time; everything else lives in stubs3/.
# usage: gen4.sh /path/to/core/src   (the directory containing itdelatrisu/)
set -e
SRC="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
O="$SRC/itdelatrisu/opsu"
mkdir -p "$OUT/stub/itdelatrisu/opsu"

extract() { awk -v pat="$2" 'f==0 && index($0,pat){f=1} f{print} f && /^\t}$/{exit}' "$1"; }

{
  echo 'package itdelatrisu.opsu; import java.util.*; public class Utils {'
  for sig in 'public static int clamp(int' 'public static float clamp(float' 'public static double clamp(double' \
             'public static float lerp(' 'public static boolean parseBoolean(' 'public static float mapDifficultyRange('; do
    extract "$O/Utils.java" "$sig"
  done
  echo '}'
} > "$OUT/stub/itdelatrisu/opsu/Utils.java"

{
  echo 'package itdelatrisu.opsu; public class GameData {'
  echo '  public enum Grade { NULL, SS, SSH, S, SH, A, B, C, D }'
  awk '/^\tpublic static final int$/{f=1} f{print} f&&/HIT_MU/{exit}' "$O/GameData.java"
  echo '}'
} > "$OUT/stub/itdelatrisu/opsu/GameData.java"

S="$HERE/stubs"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT/cls" \
  $(find "$OUT/stub" "$HERE/stubs3" -name '*.java') "$HERE/Golden4.java" \
  "$S/fluddokt/opsu/fake/Graphics.java" "$S/fluddokt/opsu/fake/Input.java" \
  "$S/itdelatrisu/opsu/ui/MenuButton.java" "$S/itdelatrisu/opsu/ui/Fonts.java" "$S/itdelatrisu/opsu/ui/animations/AnimationEquation.java" \
  "$S/itdelatrisu/opsu/render/SliderBodyRenderer.java" \
  "$SRC/fluddokt/opsu/fake/Color.java" "$SRC/fluddokt/opsu/fake/Integer2.java" \
  "$O/GameMod.java" "$O/beatmap/Beatmap.java" "$O/beatmap/LRUCache.java" "$O/beatmap/BeatmapHPDropRateCalculator.java" "$O/beatmap/BeatmapDifficultyCalculator.java" "$O/beatmap/PerformanceCalculator.java" \
  "$O/beatmap/Health.java" "$O/beatmap/HitObject.java" "$O/beatmap/TimingPoint.java" "$O"/objects/curves/*.java
timeout 600 java -cp "$OUT/cls" Golden4 "$HERE/../expected4.txt"
rm -rf "$OUT"
