#pragma once
#include <atomic>
#include "opsu/io/ReadableByteChannel.h"

namespace opsu {

/**
 * Wraps a ReadableByteChannel and counts the bytes read through it (used for download progress).
 * Port of itdelatrisu.opsu.downloads.ReadableByteChannelWrapper.
 *
 * - The wrapped channel is not owned and must outlive the wrapper.
 * - Java quirk kept: only positive results are counted (0 and the -1 end-of-stream marker are not).
 * - If the wrapped read() throws, nothing is counted.
 * - The counter is atomic (one reader thread + one writer thread is fine); read() itself is not re-entrant.
 */
class ReadableByteChannelWrapper : public ReadableByteChannel {
public:
    explicit ReadableByteChannelWrapper(ReadableByteChannel& rbc) : rbc(rbc) {}

    void close() override { rbc.close(); }
    bool isOpen() const override { return rbc.isOpen(); }

    int read(uint8_t* buf, std::size_t len) override {
        const int bytes = rbc.read(buf, len);
        if (bytes > 0)
            bytesRead.fetch_add(bytes, std::memory_order_relaxed);
        return bytes;
    }

    /** Returns the number of bytes read so far. */
    long long getReadSoFar() const { return bytesRead.load(std::memory_order_relaxed); }

private:
    ReadableByteChannel& rbc;
    std::atomic<long long> bytesRead{0};  // atomic since batch 17: Download reads it from the UI thread (see the batch 10 note)
};

}  // namespace opsu
