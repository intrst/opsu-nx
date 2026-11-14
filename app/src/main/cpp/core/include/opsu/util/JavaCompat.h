#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace opsu {
namespace java {

/** Thrown where Java threw NumberFormatException. */
struct NumberFormatException : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

/**
 * String.split(regex) for a single literal delimiter: no match -> {s};
 * otherwise trailing empty strings are removed (so ",".split(",") is empty).
 */
std::vector<std::string> split(const std::string& s, char delim);

/** String.split(regex, -1) for a single literal delimiter: no match -> {s}; trailing empty strings are KEPT. */
std::vector<std::string> splitAll(const std::string& s, char delim);

/**
 * String.trim(): strips leading / trailing chars <= U+0020. On UTF-8 text that is exactly the bytes <= 0x20
 * (every byte of a multi-byte sequence is >= 0x80). (UserList.cpp / DownloadServer.cpp keep private copies.)
 */
std::string trim(const std::string& s);

/** Integer.parseInt / Byte.parseByte / Short.parseShort (no whitespace allowed). */
int parseInt(const std::string& s);
int8_t parseByte(const std::string& s);
int16_t parseShort(const std::string& s);
/** Float.parseFloat (trims whitespace, accepts f/d suffix, NaN, Infinity). */
float parseFloat(const std::string& s);
/** Double.parseDouble (same rules as parseFloat, double precision). */
double parseDouble(const std::string& s);

/** Java's (int) cast: NaN -> 0, saturates at INT_MIN/INT_MAX (C++ is UB there). */
int toInt(double v);

/** String.format("%.Nf", v): HALF_UP on the shortest decimal repr of the double. */
std::string formatFixed(double v, int precision);
/** String.format("%g", v): precision 6, trailing zeros kept ("1000.00"). */
std::string formatGeneral(double v);
/** Float.toString(f) (JDK 19+ shortest-repr algorithm). */
std::string floatToString(float f);
/** Double.toString(d) (JDK 19+ shortest-repr algorithm). */
std::string doubleToString(double d);
/** new DecimalFormat("###.#####").format(v) generalised: HALF_EVEN, no grouping, no leading 0 (".5"). */
std::string formatTrimmed(double v, int maxFractionDigits);

/**
 * java.net.URLEncoder.encode(s, "UTF-8"): [A-Za-z0-9] and . - * _ stay, ' ' becomes '+', every other UTF-8 byte becomes %XX (upper-case hex).
 * Input is UTF-8; malformed bytes are first replaced by U+FFFD (a Java String cannot hold them), so they come out as %EF%BF%BD.
 */
std::string urlEncode(const std::string& s);

}  // namespace java
}  // namespace opsu
