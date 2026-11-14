#pragma once
#include <cstddef>
#include <cstdint>

namespace opsu {

/**
 * Minimal stand-in for java.nio.channels.ReadableByteChannel, so Download (HTTP client TBD)
 * can hand any byte source to ReadableByteChannelWrapper.
 *
 * Java's `read(ByteBuffer)` fills the buffer's remaining space; here the destination is
 * (buf, len). Implementations may throw java::IOException (JavaIO.h), like the Java methods
 * that `throws IOException`.
 */
class ReadableByteChannel {
public:
    virtual ~ReadableByteChannel() = default;

    /** Reads up to len bytes into buf. Returns the number read (may be 0) or -1 at end of stream. */
    virtual int read(uint8_t* buf, std::size_t len) = 0;
    virtual bool isOpen() const = 0;
    virtual void close() = 0;
};

}  // namespace opsu
