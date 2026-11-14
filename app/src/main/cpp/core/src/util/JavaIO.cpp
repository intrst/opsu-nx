#include "opsu/util/JavaIO.h"
#include <algorithm>

namespace opsu {
namespace java {

static size_t seqLen(unsigned char c) { return c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; }

size_t utf16Length(const std::string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size();) {
        size_t l = seqLen(static_cast<unsigned char>(s[i]));
        n += (l == 4) ? 2 : 1;
        i += l;
    }
    return n;
}

std::string utf16Prefix(const std::string& s, size_t n) {
    size_t units = 0, i = 0;
    while (i < s.size()) {
        size_t l = seqLen(static_cast<unsigned char>(s[i]));
        size_t u = (l == 4) ? 2 : 1;
        if (units + u > n) break;  // a surrogate pair is never split
        units += u;
        i += l;
    }
    return s.substr(0, std::min(i, s.size()));
}

// Mirrors the JDK's String(byte[], UTF_8) decoder (java.lang.String.decodeUTF8_UTF16 with doReplace),
// including where it differs from the Unicode "maximal subpart" practice (e.g. an encoded surrogate
// ED A0 80 is ONE U+FFFD). Verified against the real JVM by the golden tests.
std::string decodeUtf8Lossy(const std::string& s) {
    static const char FFFD[] = "\xEF\xBF\xBD";
    auto B = [&](size_t i) { return static_cast<int>(static_cast<signed char>(s[i])); };  // Java byte
    auto notCont = [](int b) { return (b & 0xc0) != 0x80; };
    std::string out;
    out.reserve(s.size());
    const size_t sl = s.size();
    size_t sp = 0;
    while (sp < sl) {
        int b1 = B(sp++);
        if (b1 >= 0) { out += static_cast<char>(b1); continue; }
        if ((b1 >> 5) == -2 && (b1 & 0x1e) != 0) {  // 2 bytes
            if (sp < sl) {
                int b2 = B(sp++);
                if (notCont(b2)) { out += FFFD; sp--; }
                else out.append(s, sp - 2, 2);
                continue;
            }
            out += FFFD;
            break;
        } else if ((b1 >> 4) == -2) {  // 3 bytes
            if (sp + 1 < sl) {
                int b2 = B(sp++), b3 = B(sp++);
                bool malformed = (b1 == static_cast<int>(static_cast<signed char>(0xe0)) && (b2 & 0xe0) == 0x80) || notCont(b2) || notCont(b3);
                if (malformed) {
                    out += FFFD;
                    size_t start = sp - 3;  // malformed3(): 1 or 2 bytes are consumed
                    int m1 = B(start), m2 = B(start + 1);
                    sp = start + (((m1 == static_cast<int>(static_cast<signed char>(0xe0)) && (m2 & 0xe0) == 0x80) || notCont(m2)) ? 1 : 2);
                } else {
                    int c = ((b1 & 0x0f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
                    if (c >= 0xD800 && c <= 0xDFFF) out += FFFD;
                    else out.append(s, sp - 3, 3);
                }
                continue;
            }
            if (sp < sl) {
                int b2 = B(sp);
                if ((b1 == static_cast<int>(static_cast<signed char>(0xe0)) && (b2 & 0xe0) == 0x80) || notCont(b2)) { out += FFFD; continue; }
            }
            out += FFFD;
            break;
        } else if ((b1 >> 3) == -2) {  // 4 bytes
            if (sp + 2 < sl) {
                int b2 = B(sp++), b3 = B(sp++), b4 = B(sp++);
                int uc = ((b1 & 0x07) << 18) | ((b2 & 0x3f) << 12) | ((b3 & 0x3f) << 6) | (b4 & 0x3f);
                if (notCont(b2) || notCont(b3) || notCont(b4) || uc < 0x10000 || uc > 0x10FFFF) {
                    out += FFFD;
                    size_t start = sp - 4;  // malformed4(): 1, 2 or 3 bytes are consumed
                    int m1 = B(start) & 0xff, m2 = B(start + 1) & 0xff;
                    size_t adv;
                    if (m1 > 0xf4 || (m1 == 0xf0 && (m2 < 0x90 || m2 > 0xbf)) || (m1 == 0xf4 && (m2 & 0xf0) != 0x80) || notCont(m2)) adv = 1;
                    else if (notCont(B(start + 2))) adv = 2;
                    else adv = 3;
                    sp = start + adv;
                } else {
                    out.append(s, sp - 4, 4);
                }
                continue;
            }
            int ub1 = b1 & 0xff;
            if (ub1 > 0xf4 || (sp < sl && ((ub1 == 0xf0 && ((B(sp) & 0xff) < 0x90 || (B(sp) & 0xff) > 0xbf)) ||
                                            (ub1 == 0xf4 && ((B(sp) & 0xff) & 0xf0) != 0x80) || notCont(B(sp) & 0xff)))) {
                out += FFFD;
                continue;
            }
            sp++;
            if (sp < sl && notCont(B(sp))) { out += FFFD; continue; }
            out += FFFD;
            break;
        } else {
            out += FFFD;  // 80..BF, C0, C1, F8..FF
        }
    }
    return out;
}

static int fold(unsigned char c) {
    if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
    return c;
}

int compareToIgnoreCase(const std::string& a, const std::string& b) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        int x = fold(static_cast<unsigned char>(a[i])), y = fold(static_cast<unsigned char>(b[i]));
        if (x != y) return x - y;
    }
    return static_cast<int>(a.size()) - static_cast<int>(b.size());
}

}  // namespace java
}  // namespace opsu
