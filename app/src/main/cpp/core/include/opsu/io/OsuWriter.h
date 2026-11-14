#pragma once
#include <cstdint>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include "opsu/util/JavaIO.h"

namespace opsu {

/**
 * Writer for osu! file types. Port of itdelatrisu.opsu.io.OsuWriter.
 *
 * Faithful to the Java, including its quirks:
 *  - write(float)/write(double) are BIG endian (DataOutputStream.writeFloat), while the integer
 *    writes and OsuReader::readSingle/readDouble are little endian.
 *  - write(string) writes the UTF-16 length and the LOW BYTE of every UTF-16 unit (writeBytes), which
 *    equals UTF-8 only for ASCII text. Non-ASCII text is therefore truncated exactly as in Java.
 * Callers must pass exact types (int8_t/int16_t/int32_t/int64_t/float/double/bool/std::string).
 */
class OsuWriter {
public:
    /** Creates/truncates a file. @throws java::FileNotFoundException */
    explicit OsuWriter(const std::string& path);
    /** Writes to an existing stream (not owned; it must outlive the writer). */
    explicit OsuWriter(std::ostream& dest);

    std::ostream& getOutputStream() { return *out; }
    void close();

    void write(int8_t v);
    void write(int16_t v);
    void write(int32_t v);
    void write(int64_t v);
    void write(float v);
    void write(double v);
    void write(bool v);
    /** Unsigned variable length integer (ULEB128). Negative values are encoded as 32-bit unsigned
     *  (Java's arithmetic shift would loop forever on them). */
    void writeULEB128(int32_t i);
    void write(const std::string& s);
    void write(const char*) = delete;  // would silently pick write(bool)
    /** Writes a date (milliseconds since the Unix epoch) as Windows ticks. */
    void writeDate(int64_t millis);
    void write(const std::vector<uint8_t>& b);

private:
    void raw(const uint8_t* p, size_t n);

    std::unique_ptr<std::ofstream> owned;
    std::ostream* out;
};

}  // namespace opsu
