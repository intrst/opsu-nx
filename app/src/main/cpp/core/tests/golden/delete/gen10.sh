#!/bin/sh
# Regenerates ../expected10.txt from the running JDK (tenth golden suite: Utils.deleteToTrash / deleteDirectory on a real file tree). Needs a POSIX sh, find, ln.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; WORK="$(mktemp -d)"; ROOT="$WORK/root"; mkdir "$ROOT"
java -m jdk.compiler/com.sun.tools.javac.Main -nowarn -d "$WORK/cls" "$HERE/DeleteRunner.java"
( cd "$ROOT" && LC_ALL=C.UTF-8 java -cp "$WORK/cls" DeleteRunner "$HERE/scenarios.txt" "$HERE/fixture.sh" "$HERE/../expected10.txt" )
rm -rf "$WORK"
