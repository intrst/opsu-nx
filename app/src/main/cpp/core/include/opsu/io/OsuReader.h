#pragma once
#include <cstdint>
#include <fstream>
#include <istream>
#include <memory>
#include <string>
#include "opsu/util/JavaIO.h"

namespace opsu {

/**
 * Reader for osu! file types (little endian). Port of itdelatrisu.opsu.io.OsuReader.
 * Short reads throw java::EOFException; format errors throw java::IOException.
 */
class OsuReader {
public:
    /** Opens a file. @throws java::FileNotFoundException */
    explicit OsuReader(const std::string& path);
    /** Reads from an existing stream (not owned; it must outlive the reader). */
    explicit OsuReader(std::istream& source);

    std::istream& getInputStream() { return *in; }
    void close();

    int8_t readByte();
    int16_t readShort();
    int32_t readInt();
    int64_t readLong();
    float readSingle();
    double readDouble();
    bool readBoolean();
    /** Unsigned variable length integer (ULEB128). */
    int32_t readULEB128();
    /** Variable-length UTF-8 string. */
    std::string readString();
    /** An 8-byte Windows-ticks date, as milliseconds since the Unix epoch (java.util.Date#getTime). */
    int64_t readDate();

private:
    void readFully(uint8_t* buf, size_t n);

    std::unique_ptr<std::ifstream> owned;
    std::istream* in;
};

}  // namespace opsu
