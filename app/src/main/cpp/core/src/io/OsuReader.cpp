#include "opsu/io/OsuReader.h"
#include <cstdio>
#include <cstring>

namespace opsu {

OsuReader::OsuReader(const std::string& path) {
    owned.reset(new std::ifstream(path, std::ios::binary));
    if (!*owned) throw java::FileNotFoundException(path + " (No such file or directory)");
    in = owned.get();
}

OsuReader::OsuReader(std::istream& source) : in(&source) {}

void OsuReader::close() {
    if (owned) owned->close();
}

void OsuReader::readFully(uint8_t* buf, size_t n) {
    in->read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(n));
    if (static_cast<size_t>(in->gcount()) != n) throw java::EOFException();
}

int8_t OsuReader::readByte() {
    uint8_t b;
    readFully(&b, 1);
    return static_cast<int8_t>(b);
}

int16_t OsuReader::readShort() {
    uint8_t b[2];
    readFully(b, 2);
    return static_cast<int16_t>(static_cast<uint16_t>(b[0] | (b[1] << 8)));
}

int32_t OsuReader::readInt() {
    uint8_t b[4];
    readFully(b, 4);
    return static_cast<int32_t>(static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
                                (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24));
}

int64_t OsuReader::readLong() {
    uint8_t b[8];
    readFully(b, 8);
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | b[i];
    return static_cast<int64_t>(v);
}

float OsuReader::readSingle() {
    int32_t i = readInt();
    float f;
    std::memcpy(&f, &i, 4);
    return f;
}

double OsuReader::readDouble() {
    int64_t l = readLong();
    double d;
    std::memcpy(&d, &l, 8);
    return d;
}

bool OsuReader::readBoolean() { return readByte() != 0; }

int32_t OsuReader::readULEB128() {
    uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 7) {
        int8_t b = readByte();
        value |= static_cast<uint32_t>(b & 0x7F) << shift;
        if (b >= 0) return static_cast<int32_t>(value);  // MSB is zero. End of value.
    }
    throw java::IOException("ULEB128 too large");
}

std::string OsuReader::readString() {
    // 00 = empty string; 0B <length> <char>* = normal string (length = byte length, UTF-8 content)
    int8_t kind = readByte();
    if (kind == 0) return "";
    if (kind != 0x0B) {
        char msg[80];
        std::snprintf(msg, sizeof msg, "String format error: Expected 0x0B or 0x00, found 0x%02X", kind & 0xFF);
        throw java::IOException(msg);
    }
    int32_t length = readULEB128();
    if (length == 0) return "";
    if (length < 0) throw java::NegativeArraySizeException(std::to_string(length));  // Java: new byte[length]
    std::string s(static_cast<size_t>(length), '\0');
    readFully(reinterpret_cast<uint8_t*>(&s[0]), static_cast<size_t>(length));
    return java::decodeUtf8Lossy(s);  // new String(bytes, "UTF-8") never throws
}

int64_t OsuReader::readDate() {
    const int64_t TICKS_AT_EPOCH = 621355968000000000LL;
    const int64_t TICKS_PER_MILLISECOND = 10000;
    int64_t ticks = readLong();
    int64_t diff = static_cast<int64_t>(static_cast<uint64_t>(ticks) - static_cast<uint64_t>(TICKS_AT_EPOCH));
    return diff / TICKS_PER_MILLISECOND;
}

}  // namespace opsu
