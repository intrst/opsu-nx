#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "opsu/util/JavaCompat.h"

namespace opsu {
/** Pure subset of itdelatrisu.opsu.Utils (the rest is engine/IO and is ported with its users). */
namespace Utils {

inline int clamp(int val, int low, int high) { return val < low ? low : (val > high ? high : val); }
inline float clamp(float val, float low, float high) { return val < low ? low : (val > high ? high : val); }
inline double clamp(double val, double low, double high) { return val < low ? low : (val > high ? high : val); }

/** Linear interpolation of a and b at t. */
inline float lerp(float a, float b, float t) { return a * (1 - t) + b * t; }

/** Java: Integer.parseInt(s) == 1. Throws java::NumberFormatException. */
inline bool parseBoolean(const std::string& s) { return java::parseInt(s) == 1; }


/** Euclidean distance between (x1,y1) and (x2,y2). */
inline float distance(float x1, float y1, float x2, float y2) {
    float v1 = std::fabs(x1 - x2);
    float v2 = std::fabs(y1 - y2);
    return static_cast<float>(std::sqrt(static_cast<double>((v1 * v1) + (v2 * v2))));
}

/** Standard deviation (float accumulation exactly as the Java). Empty list -> NaN, like Java. */
double standardDeviation(const std::vector<int>& list);

/** Maps a difficulty value to the given range (peppy, ppy/osu-iPhone:OsuFunctions.m). */
inline float mapDifficultyRange(float difficulty, float min, float mid, float max) {
    if (difficulty > 5.f)
        return mid + (max - mid) * (difficulty - 5.f) / 5.f;
    else if (difficulty < 5.f)
        return mid - (mid - min) * (5.f - difficulty) / 5.f;
    else
        return mid;
}

/** "1 second" / "N seconds" / "mm:ss" / "hh:mm:ss". */
std::string getTimeString(int seconds);

/** Human-readable byte count ("1.5 KB"). */
std::string bytesToString(long long bytes);

/**
 * Removes characters that are illegal in file names (or replaces them with `replace`, if > 0 and
 * itself legal). Operates on UTF-8 bytes; every illegal character is ASCII so this is equivalent.
 */
std::string cleanFileName(const std::string& badFileName, char replace);

/** Utils.copyFile: copies src over dst (UTF-8 paths). Throws java::IOException. */
void copyFile(const std::string& src, const std::string& dst);

/** Utils.moveFile: rename, else copy + delete the source (e.g. across file systems). Throws java::IOException. */
void moveFile(const std::string& src, const std::string& dst);

/**
 * Utils.deleteDirectory: recursively deletes the files and folders in a directory, then the directory itself. A path that is not a directory is ignored.
 * Like Java's `File.delete()` every failure is silent (the entry just stays). Java quirk kept: `File.isDirectory()` follows symbolic links, so a symlink
 * to a directory is entered and ITS TARGET'S contents are deleted before the link itself is removed (see PORTING_NOTES batch 36).
 */
void deleteDirectory(const std::string& dir);

/**
 * Utils.deleteToTrash: deletes a file or a directory tree (the Java moves to the system trash only in commented-out code, so it always deletes and returns false;
 * the return value is "moved to trash", kept for the callers). `path` is UTF-8. Java's null file cannot exist here.
 * @throws java::IOException "File '<absolute path>' does not exist." when the path does not exist (a broken symlink counts as not existing, like `File.exists()`);
 *         the absolute path is the working directory + the path with duplicate / trailing '/' removed, as `File.getAbsolutePath()`.
 */
bool deleteToTrash(const std::string& path);

}  // namespace Utils
}  // namespace opsu
