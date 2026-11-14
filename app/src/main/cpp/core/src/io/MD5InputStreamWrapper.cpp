#include "opsu/io/MD5InputStreamWrapper.h"
#include <stdexcept>
#include <vector>

namespace opsu {

int MD5InputStreamWrapper::read() {
    int c = in.get();
    if (c == std::istream::traits_type::eof()) {
        eof = true;
        return -1;
    }
    uint8_t b = (uint8_t)c;
    md.update(&b, 1);
    return c;
}

int MD5InputStreamWrapper::read(uint8_t* buf, std::size_t len) {
    if (len == 0) return 0;
    in.read(reinterpret_cast<char*>(buf), (std::streamsize)len);
    std::streamsize n = in.gcount();
    if (n <= 0) {
        eof = true;
        return -1;
    }
    md.update(buf, (std::size_t)n);
    return (int)n;
}

std::streamsize MD5InputStreamWrapper::available() {
    std::streamsize n = in.rdbuf() ? in.rdbuf()->in_avail() : 0;
    return n < 0 ? 0 : n;
}

void MD5InputStreamWrapper::reset() { throw std::runtime_error("The reset() method is not implemented."); }

long long MD5InputStreamWrapper::skip(long long) { throw std::runtime_error("The skip() method is not implemented."); }

std::string MD5InputStreamWrapper::getMD5() {
    if (md5Done) return md5;
    if (!eof) {  // read the rest of the stream
        std::vector<uint8_t> buf(0x1000);
        while (!eof) read(buf.data(), buf.size());
    }
    md5 = md.hexDigest();
    md5Done = true;
    return md5;
}

}  // namespace opsu
