#!/bin/sh
# Regenerates ../expected12.txt from the running JDK (twelfth golden suite: Storyboard data model + evaluation).
# usage: gen12.sh <path to the Java core/src dir>   (needs a JDK with the jdk.compiler module: java -m jdk.compiler/com.sun.tools.javac.Main)
set -e
SRC="${1:?usage: gen12.sh <core/src dir>}"
HERE="$(cd "$(dirname "$0")" && pwd)"; WORK="$(mktemp -d)"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$WORK/cls" \
  "$SRC/itdelatrisu/opsu/storyboard/Storyboard.java" "$SRC/itdelatrisu/opsu/ui/animations/AnimationEquation.java" "$HERE/StoryboardGolden.java"
java -cp "$WORK/cls" itdelatrisu.opsu.storyboard.StoryboardGolden "$HERE/../expected12.txt"
rm -rf "$WORK"
