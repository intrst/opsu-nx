// Tests for batch 10 (Resolution, ReadableByteChannelWrapper, UselessUtils screenshot helpers).
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <vector>
#include "opsu/Resolution.h"
#include "opsu/downloads/ReadableByteChannelWrapper.h"
#include "opsu/kww/UselessUtils.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

/** Channel that replays a scripted list of results (a value > 0 = that many bytes, 0, -1 = EOF, or a throw). */
struct ScriptedChannel : ReadableByteChannel {
    std::deque<int> script;   // INT_MIN-like sentinel below = throw
    static constexpr int THROW = -1000;
    bool open = true;
    int closeCalls = 0;
    int read(uint8_t* buf, std::size_t len) override {
        int r = script.front(); script.pop_front();
        if (r == THROW) throw java::IOException("boom");
        for (int i = 0; i < r && (std::size_t)i < len; i++) buf[i] = (uint8_t)(i + 1);
        return r;
    }
    bool isOpen() const override { return open; }
    void close() override { open = false; closeCalls++; }
};

int main() {
    // Resolution
    {
        Resolution r(1920, 1080);
        CHECK(r.getWidth() == 1920 && r.getHeight() == 1080);
        const Resolution z(0, -1);
        CHECK(z.getWidth() == 0 && z.getHeight() == -1);
    }

    // wrapper: counts only positive results, passes data and return values through unchanged
    {
        ScriptedChannel ch;
        ch.script = {10, 0, 5, -1, 7, -1};
        ReadableByteChannelWrapper w(ch);
        uint8_t buf[16] = {};
        CHECK(w.getReadSoFar() == 0);
        CHECK(w.read(buf, 16) == 10 && w.getReadSoFar() == 10);
        CHECK(buf[0] == 1 && buf[9] == 10);              // data reached the caller's buffer
        CHECK(w.read(buf, 16) == 0 && w.getReadSoFar() == 10);    // 0 is not counted
        CHECK(w.read(buf, 16) == 5 && w.getReadSoFar() == 15);
        CHECK(w.read(buf, 16) == -1 && w.getReadSoFar() == 15);   // EOF marker is not subtracted
        CHECK(w.read(buf, 16) == 7 && w.getReadSoFar() == 22);    // keeps counting after a -1
    }
    // wrapper: a throwing read counts nothing and the exception propagates
    {
        ScriptedChannel ch;
        ch.script = {4, ScriptedChannel::THROW, 6};
        ReadableByteChannelWrapper w(ch);
        uint8_t buf[8];
        w.read(buf, 8);
        bool threw = false;
        try { w.read(buf, 8); } catch (const java::IOException&) { threw = true; }
        CHECK(threw && w.getReadSoFar() == 4);
        CHECK(w.read(buf, 8) == 6 && w.getReadSoFar() == 10);
    }
    // wrapper: close / isOpen forward to the wrapped channel; works through the base interface
    {
        ScriptedChannel ch;
        ReadableByteChannelWrapper w(ch);
        ReadableByteChannel& base = w;
        CHECK(base.isOpen() && ch.isOpen());
        base.close();
        CHECK(!base.isOpen() && !ch.isOpen() && ch.closeCalls == 1);
    }
    // wrapper: totals above 2^31 do not overflow (Java: long bytesRead)
    {
        ScriptedChannel ch;
        for (int i = 0; i < 3; i++) ch.script.push_back(2000000000);
        ReadableByteChannelWrapper w(ch);
        uint8_t b;
        for (int i = 0; i < 3; i++) w.read(&b, 1);
        CHECK(w.getReadSoFar() == 6000000000LL);
    }

    // makeOpaque: alpha bytes only; trailing partial pixel untouched; sizes 0..3 do nothing
    {
        uint8_t px[11];
        std::memset(px, 7, sizeof px);
        kww::makeOpaque(px, sizeof px);   // pixels 0 and 1 complete (alpha at 3, 7); bytes 8..10 partial
        for (int i = 0; i < 11; i++) CHECK(px[i] == ((i == 3 || i == 7) ? 255 : 7));
        uint8_t small[4] = {1, 2, 3, 4};
        kww::makeOpaque(small, 3);        // i = 3 is not < 3
        CHECK(small[0] == 1 && small[1] == 2 && small[2] == 3 && small[3] == 4);
        kww::makeOpaque(small, 0);
        CHECK(small[3] == 4);
        kww::makeOpaque(small, 4);        // exactly one pixel
        CHECK(small[3] == 255 && small[2] == 3);
        std::vector<uint8_t> big(4 * 64 * 64, 0);
        kww::makeOpaque(big.data(), big.size());
        bool ok = true;
        for (size_t i = 0; i < big.size(); i++) ok &= big[i] == ((i % 4 == 3) ? 255 : 0);
        CHECK(ok);
    }

    // screenshot file name: yyyyMMdd_HHmmss, zero padded, 24-hour clock
    {
        std::tm t{};
        t.tm_year = 2026 - 1900; t.tm_mon = 10; t.tm_mday = 13; t.tm_hour = 9; t.tm_min = 5; t.tm_sec = 7;
        CHECK(kww::screenshotFileName(t) == "screenshot_20261113_090507.png");
        t.tm_hour = 23; t.tm_min = 59; t.tm_sec = 59; t.tm_mon = 0; t.tm_mday = 1;
        CHECK(kww::screenshotFileName(t) == "screenshot_20260101_235959.png");
        t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
        CHECK(kww::screenshotFileName(t) == "screenshot_20260101_000000.png");   // midnight is 00, not 24 or 12
        t.tm_year = 5 - 1900;
        CHECK(kww::screenshotFileName(t) == "screenshot_00050101_000000.png");   // yyyy pads to 4 digits
        // time_t overload agrees with the broken-down one (local zone), whatever the zone is
        std::time_t now = 1700000000;
        std::tm lt = *std::localtime(&now);
        CHECK(kww::screenshotFileName(now) == kww::screenshotFileName(lt));
        CHECK(kww::screenshotFileName(now).size() == std::strlen("screenshot_20231114_221320.png"));
    }

    if (failures) { std::printf("batch10: %d FAILED\n", failures); return 1; }
    std::printf("batch10: all tests passed\n");
    return 0;
}
