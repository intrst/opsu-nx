#pragma once
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>
#include "opsu/util/MD5.h"

namespace opsu {

/**
 * Wraps an input stream and computes its MD5 while it is read.
 * Port of itdelatrisu.opsu.io.MD5InputStreamWrapper (java.io.InputStream -> std::istream).
 *
 * - The stream is not owned and must outlive the wrapper.
 * - read() returns the byte (0..255) or -1 at end of stream; read(buf, len) returns the number of
 *   bytes read, 0 for len == 0, or -1 at end of stream (Java InputStream contract).
 * - skip() and reset() throw std::runtime_error, like the Java RuntimeException.
 * - The "NoSuchAlgorithmException" of the Java constructor cannot happen here (built-in MD5).
 */
class MD5InputStreamWrapper {
public:
    explicit MD5InputStreamWrapper(std::istream& in) : in(in) {}

    int read();
    int read(uint8_t* buf, std::size_t len);
    /** Bytes that can be read without blocking (InputStream.available()); 0 when unknown. */
    std::streamsize available();
    bool markSupported() const { return false; }
    /** @throws std::runtime_error always (Java: RuntimeException "not implemented"). */
    void reset();
    /** @throws std::runtime_error always. */
    long long skip(long long n);

    /** MD5 of the whole stream as lowercase hex; reads the rest of the stream first if needed. */
    std::string getMD5();

private:
    std::istream& in;
    bool eof = false;
    MD5 md;
    std::string md5;
    bool md5Done = false;
};

}  // namespace opsu
