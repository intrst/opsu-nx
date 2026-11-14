#pragma once
#include <stdexcept>
#include <string>

namespace opsu {
namespace java {

/** java.io.IOException */
struct IOException : std::runtime_error {
    using std::runtime_error::runtime_error;
};
/** java.io.EOFException (end of stream reached before a full value was read). */
struct EOFException : IOException {
    EOFException() : IOException("EOF") {}
};
/** java.io.FileNotFoundException */
struct FileNotFoundException : IOException {
    using IOException::IOException;
};

/** java.lang.NegativeArraySizeException (a RuntimeException, deliberately not an IOException). */
struct NegativeArraySizeException : std::runtime_error {
    explicit NegativeArraySizeException(const std::string& m) : std::runtime_error(m) {}
};

/** Number of UTF-16 code units of a UTF-8 string (Java String.length()). */
size_t utf16Length(const std::string& utf8);
/** Java s.substring(0, n) on the UTF-16 view of a UTF-8 string (n clamped; never splits a code point). */
std::string utf16Prefix(const std::string& utf8, size_t n);
/**
 * new String(bytes, "UTF-8"): malformed input is replaced by U+FFFD, one per maximal invalid subpart
 * (Unicode best practice, which is what the JDK does). The result is always valid UTF-8.
 */
std::string decodeUtf8Lossy(const std::string& bytes);
/** Java String.compareToIgnoreCase for ASCII text (non-ASCII bytes are compared as-is). */
int compareToIgnoreCase(const std::string& a, const std::string& b);

}  // namespace java
}  // namespace opsu
