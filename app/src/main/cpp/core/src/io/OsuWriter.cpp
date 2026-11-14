#include "opsu/io/OsuWriter.h"
#include <cstring>

namespace opsu {

OsuWriter::OsuWriter(const std::string& path) {
    owned.reset(new std::ofstream(path, std::ios::binary | std::ios::trunc));
    if (!*owned) throw java::FileNotFoundException(path + " (cannot open for writing)");
    out = owned.get();
}

OsuWriter::OsuWriter(std::ostream& dest) : out(&dest) {}

void OsuWriter::close() {
    out->flush();
    if (owned) owned->close();
}

void OsuWriter::raw(const uint8_t* p, size_t n) {
    out->write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n));
    if (!*out) throw java::IOException("write failed");
}

void OsuWriter::write(int8_t v) { raw(reinterpret_cast<uint8_t*>(&v), 1); }

void OsuWriter::write(int16_t v) {
    uint16_t u = static_cast<uint16_t>(v);
    uint8_t b[2] = {static_cast<uint8_t>(u), static_cast<uint8_t>(u >> 8)};
    raw(b, 2);
}

void OsuWriter::write(int32_t v) {
    uint32_t u = static_cast<uint32_t>(v);
    uint8_t b[4];
    for (int i = 0; i < 4; i++) b[i] = static_cast<uint8_t>(u >> (8 * i));
    raw(b, 4);
}

void OsuWriter::write(int64_t v) {
    uint64_t u = static_cast<uint64_t>(v);
    uint8_t b[8];
    for (int i = 0; i < 8; i++) b[i] = static_cast<uint8_t>(u >> (8 * i));
    raw(b, 8);
}

void OsuWriter::write(float v) {  // big endian, canonical NaN (DataOutputStream.writeFloat)
    uint32_t u;
    if (v != v) u = 0x7fc00000u; else std::memcpy(&u, &v, 4);
    uint8_t b[4];
    for (int i = 0; i < 4; i++) b[i] = static_cast<uint8_t>(u >> (8 * (3 - i)));
    raw(b, 4);
}

void OsuWriter::write(double v) {  // big endian, canonical NaN (DataOutputStream.writeDouble)
    uint64_t u;
    if (v != v) u = 0x7ff8000000000000ull; else std::memcpy(&u, &v, 8);
    uint8_t b[8];
    for (int i = 0; i < 8; i++) b[i] = static_cast<uint8_t>(u >> (8 * (7 - i)));
    raw(b, 8);
}

void OsuWriter::write(bool v) { write(static_cast<int8_t>(v ? 1 : 0)); }

void OsuWriter::writeULEB128(int32_t i) {
    uint32_t value = static_cast<uint32_t>(i);
    do {
        uint8_t b = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0) b |= (1 << 7);
        raw(&b, 1);
    } while (value != 0);
}

void OsuWriter::write(const std::string& s) {
    size_t len = java::utf16Length(s);
    if (len == 0) {
        write(static_cast<int8_t>(0x00));
        return;
    }
    write(static_cast<int8_t>(0x0B));
    writeULEB128(static_cast<int32_t>(len));
    // writeBytes(s): low byte of each UTF-16 code unit.
    std::vector<uint8_t> bytes;
    bytes.reserve(len);
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp;
        size_t l;
        if (c < 0x80) { cp = c; l = 1; }
        else if (c < 0xE0) { cp = c & 0x1F; l = 2; }
        else if (c < 0xF0) { cp = c & 0x0F; l = 3; }
        else { cp = c & 0x07; l = 4; }
        for (size_t k = 1; k < l && i + k < s.size(); k++) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        i += l;
        if (cp >= 0x10000) {
            uint32_t v = cp - 0x10000;
            uint16_t hi = static_cast<uint16_t>(0xD800 + (v >> 10)), lo = static_cast<uint16_t>(0xDC00 + (v & 0x3FF));
            bytes.push_back(static_cast<uint8_t>(hi));
            bytes.push_back(static_cast<uint8_t>(lo));
        } else {
            bytes.push_back(static_cast<uint8_t>(cp));
        }
    }
    raw(bytes.data(), bytes.size());
}

void OsuWriter::writeDate(int64_t millis) {
    const int64_t TICKS_AT_EPOCH = 621355968000000000LL;
    const int64_t TICKS_PER_MILLISECOND = 10000;
    int64_t ticks = static_cast<int64_t>(static_cast<uint64_t>(TICKS_AT_EPOCH) +
                                         static_cast<uint64_t>(millis) * static_cast<uint64_t>(TICKS_PER_MILLISECOND));
    write(ticks);
}

void OsuWriter::write(const std::vector<uint8_t>& b) { raw(b.data(), b.size()); }

}  // namespace opsu
