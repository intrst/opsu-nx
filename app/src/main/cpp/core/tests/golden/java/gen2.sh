#!/bin/sh
# Regenerates ../expected2.txt from the REAL Java sources (second golden suite: animations, user,
# health, Utils helpers, OsuReader/Writer). The Utils/GameData stubs are *extracted from the real
# files* at generation time, so their method bodies are never hand-copied.
# usage: gen2.sh /path/to/core/src   (the directory containing itdelatrisu/)
set -e
SRC="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$(mktemp -d)"
O="$SRC/itdelatrisu/opsu"
mkdir -p "$OUT/stub/itdelatrisu/opsu"

# prints a method (from its signature line to the closing "\t}")
extract() { awk -v pat="$2" 'f==0 && index($0,pat){f=1} f{print} f && /^\t}$/{exit}' "$1"; }

{
  echo 'package itdelatrisu.opsu; import java.util.*; public class Utils {'
  awk '/private final static int\[\] illegalChars/{f=1} f{print} f&&/static \{/{s=1} s&&/^\t}$/{exit}' "$O/Utils.java"
  for sig in 'public static int clamp(int' 'public static float clamp(float' 'public static double clamp(double' \
             'public static float distance(' 'public static float lerp(' 'public static double standardDeviation(' \
             'public static float mapDifficultyRange(' 'public static String bytesToString(' \
             'public static String cleanFileName(' 'public static String getTimeString('; do
    extract "$O/Utils.java" "$sig"
  done
  echo '}'
} > "$OUT/stub/itdelatrisu/opsu/Utils.java"

{
  echo 'package itdelatrisu.opsu; public class GameData {'
  echo '  public enum Grade { NULL, SS, SSH, S, SH, A, B, C, D }'
  awk '/^\tpublic static final int$/{f=1} f{print} f&&/HIT_MU/{exit}' "$O/GameData.java"
  extract "$O/GameData.java" 'public static float getScorePercent(int'
  extract "$O/GameData.java" 'public static Grade getGrade(int'
  echo '}'
} > "$OUT/stub/itdelatrisu/opsu/GameData.java"

java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$OUT/cls" \
  $(find "$OUT/stub" "$HERE/stubs2" -name '*.java') "$HERE/Golden2.java" \
  "$O/GameMod.java" "$O/user/User.java" "$O/user/UserStats.java" "$O/beatmap/Health.java" \
  "$O/ui/KineticScrolling.java" "$O/ui/animations/"*.java "$O/io/OsuReader.java" "$O/io/OsuWriter.java" \
  $(ls "$HERE"/stubs/itdelatrisu/opsu/options/Options.java "$HERE"/stubs/itdelatrisu/opsu/ui/MenuButton.java "$HERE"/stubs/itdelatrisu/opsu/ui/Fonts.java "$HERE"/stubs/itdelatrisu/opsu/GameImage.java "$HERE"/stubs/fluddokt/opsu/fake/Input.java "$HERE"/stubs/fluddokt/opsu/fake/Graphics.java "$HERE"/stubs/fluddokt/opsu/fake/Color.java "$HERE"/stubs/fluddokt/opsu/fake/Log.java "$HERE"/stubs/fluddokt/opsu/fake/Image.java)
java -cp "$OUT/cls" Golden2 "$HERE/../expected2.txt"
rm -rf "$OUT"
