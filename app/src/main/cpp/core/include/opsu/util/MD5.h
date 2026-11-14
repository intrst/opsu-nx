#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace opsu {

/**
 * Minimal RFC 1321 MD5, standing in for java.security.MessageDigest.getInstance("MD5")
 * (the C++ standard library has no digest). Streaming: update() any number of times, digest() once.
 */
class MD5 {
public:
    MD5();
    void update(const uint8_t* data, std::size_t len);
    /** Finishes and returns the 16 raw bytes, like MessageDigest.digest(). Resets the state afterwards, like Java. */
    std::array<uint8_t, 16> digest();
    /** Lowercase hex of digest(), i.e. Java's String.format("%02x") per byte. */
    std::string hexDigest();

private:
    void reset();
    void transform(const uint8_t block[64]);

    uint32_t state[4];
    uint64_t totalBytes;
    uint8_t buffer[64];
};

}  // namespace opsu
