// Tests for batch 5 (SoundEffect, MD5, MD5InputStreamWrapper).
// Expected MD5 values come from Python hashlib over the same byte pattern.
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "opsu/audio/SoundEffect.h"
#include "opsu/io/MD5InputStreamWrapper.h"
#include "opsu/util/MD5.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static std::string pat(size_t n) {
    std::string s(n, '\0');
    for (size_t i = 0; i < n; i++) s[i] = (char)((i * 31 + 7) & 255);
    return s;
}

struct Case { size_t len; const char* md5; };
static const Case cases[] = {
    {0, "d41d8cd98f00b204e9800998ecf8427e"},
    {1, "89e74e640b8c46257a29de0616794d5d"},
    {3, "3b704fb2660febf62921a0caaa8de56b"},
    {55, "c9e512626618c9980ef21a96597af94c"},
    {56, "ecde7caa08e9f5657c863df107cac60a"},
    {57, "b17b1a018dd6a4d1edda8aca15f17846"},
    {63, "2f0301069e1c40af7f6c8f843b1b13f2"},
    {64, "b6bf87c24b1bc334e2541387a92b981b"},
    {65, "f168246f08b6134d66bd2a10343fa9f1"},
    {119, "d5dc3d8264de3aa24dee105910ee27fe"},
    {120, "bcc139b3848923904860d1eefd6e5923"},
    {127, "27670172396247fa5f68637f13f8933d"},
    {128, "3e85b70ffc8df5c735ecf2a8f14f1bee"},
    {1000, "2b1e78d5765de9e10495a01412a1cf22"},
    {4096, "88c3b31b216d705c77b752990f1b55f5"},
    {4097, "90af61113bbfa322fccc11b4c75f4808"},
    {100000, "7354f16d06a013232daa23cfa7a092df"},
};

static std::string md5Of(const std::string& s) {
    MD5 m;
    m.update(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    return m.hexDigest();
}

int main() {
    // RFC 1321 vectors
    CHECK(md5Of("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(md5Of("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5Of("The quick brown fox jumps over the lazy dog") == "9e107d9d372bb6826bd81d3542a419d6");

    // padding boundaries, one-shot and fed in awkward chunk sizes
    for (const Case& c : cases) {
        std::string s = pat(c.len);
        CHECK(md5Of(s) == c.md5);
        for (size_t chunk : {1u, 7u, 63u, 64u, 65u}) {
            MD5 m;
            for (size_t i = 0; i < s.size(); i += chunk)
                m.update(reinterpret_cast<const uint8_t*>(s.data()) + i, std::min(chunk, s.size() - i));
            CHECK(m.hexDigest() == c.md5);
        }
    }

    // digest() resets the state (Java MessageDigest.digest() semantics)
    {
        MD5 m;
        m.update(reinterpret_cast<const uint8_t*>("abc"), 3);
        m.hexDigest();
        CHECK(m.hexDigest() == "d41d8cd98f00b204e9800998ecf8427e");
    }

    // wrapper: getMD5 with nothing read yet reads the whole stream
    for (const Case& c : cases) {
        std::istringstream in(pat(c.len));
        MD5InputStreamWrapper w(in);
        CHECK(w.getMD5() == c.md5);
        CHECK(w.getMD5() == c.md5);  // cached
    }

    // wrapper: partial reads (single bytes + block read) then getMD5 finishes the rest
    {
        std::string s = pat(10000);
        std::istringstream in(s);
        MD5InputStreamWrapper w(in);
        CHECK(w.read() == (unsigned char)s[0]);
        CHECK(w.read() == (unsigned char)s[1]);
        std::vector<uint8_t> buf(300);
        CHECK(w.read(buf.data(), buf.size()) == 300);
        CHECK(std::memcmp(buf.data(), s.data() + 2, 300) == 0);
        CHECK(w.read(buf.data(), 0) == 0);
        CHECK(w.getMD5() == md5Of(s));
    }

    // wrapper: read() / read(buf) return -1 at EOF; a short final read returns the remainder
    {
        std::istringstream in(pat(5));
        MD5InputStreamWrapper w(in);
        std::vector<uint8_t> buf(16);
        CHECK(w.read(buf.data(), buf.size()) == 5);
        CHECK(w.read(buf.data(), buf.size()) == -1);
        CHECK(w.read() == -1);
        CHECK(w.getMD5() == md5Of(pat(5)));
    }

    // wrapper: unsupported operations
    {
        std::istringstream in("x");
        MD5InputStreamWrapper w(in);
        CHECK(!w.markSupported());
        bool a = false, b = false;
        try { w.reset(); } catch (const std::runtime_error&) { a = true; }
        try { w.skip(1); } catch (const std::runtime_error&) { b = true; }
        CHECK(a && b);
    }

    // SoundEffect
    CHECK(SoundEffect_SIZE == soundEffectValues().size());
    CHECK(std::string(getFileName(SoundEffect::APPLAUSE)) == "applause");
    CHECK(std::string(getFileName(SoundEffect::COUNT1)) == "count1s");
    CHECK(std::string(getFileName(SoundEffect::SPINNEROSU)) == "spinner-osu");
    CHECK(static_cast<int>(SoundEffect::SPINNERSPIN) == (int)SoundEffect_SIZE - 1);
    {
        int i = 0;
        for (SoundEffect e : soundEffectValues()) {
            CHECK(static_cast<int>(e) == i++);
            CHECK(getFileName(e)[0] != '\0');
        }
    }

    if (failures == 0) std::printf("batch5: all tests passed\n");
    return failures ? 1 : 0;
}
