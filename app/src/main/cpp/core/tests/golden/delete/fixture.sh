#!/bin/sh
# Rebuilds the fixture tree used by the deleteToTrash comparison (Java side: gen10.sh, C++ side: tests/test_golden10.cpp) INSIDE the given directory,
# which is emptied first (the directory itself is kept: it is the working directory of the run).
set -e
d="$1"
find "$d" -mindepth 1 -delete
mkdir -p "$d/ext/deep" "$d/tree/sub/deeper" "$d/empty" "$d/withlink" "$d/日本" "$d/dir with space"
echo a > "$d/f.txt"
echo b > "$d/tree/one.txt"
echo c > "$d/tree/sub/two.osu"
echo d > "$d/tree/sub/deeper/three.mp3"
mkdir "$d/hiddendir"
: > "$d/hiddendir/.hidden"
echo e > "$d/ext/outside.txt"
echo f > "$d/ext/deep/outside2.txt"
echo g > "$d/日本/曲.txt"
echo h > "$d/dir with space/a b.txt"
ln -s nowhere "$d/broken"
ln -s f.txt "$d/linkfile"
ln -s ext "$d/linkdir"
ln -s ../ext "$d/withlink/extlink"
echo i > "$d/withlink/plain.txt"
ln -s ../f.txt "$d/withlink/filelink"
