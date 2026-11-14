// Behavioural tests for batch 22: util/JavaDate (the SimpleDateFormat slice of the download servers). The JDK comparison is test_golden9.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <string>
#include "opsu/util/JavaDate.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main() {
    setenv("TZ", "Asia/Jakarta", 1);  // UTC+7, no DST: the author's zone
    tzset();
    const std::string ISO = "yyyy-MM-dd'T'HH:mm:ss";
    // Catboy / Hinamizawa / Nekoha: UTC in, default zone out
    CHECK(java::reformatDate("2023-05-14T23:38:47", ISO, true) == "15 May 2023 06:38:47");
    CHECK(java::reformatDate("garbage", ISO, true) == "garbage");
    CHECK(java::reformatDate("", ISO, true) == "");
    // OsuMirror: quoted 'Z' is a literal
    CHECK(java::reformatDate("2015-05-14T23:38:47Z", "yyyy-MM-dd'T'HH:mm:ss'Z'", true) == "15 May 2015 06:38:47");
    CHECK(java::reformatDate("2015-05-14T23:38:47", "yyyy-MM-dd'T'HH:mm:ss'Z'", true) == "2015-05-14T23:38:47");  // missing Z: raw string
    // Bloodcat: default zone in and out, so the wall clock is unchanged; the fraction is parsed but not printed
    CHECK(java::reformatDate("2015-09-30 09:39:04.536", "yyyy-MM-dd HH:mm:ss.SSS", false) == "30 Sep 2015 09:39:04");
    CHECK(java::reformatDate("2015-09-30 09:39:04", "yyyy-MM-dd HH:mm:ss.SSS", false) == "2015-09-30 09:39:04");  // no ".SSS": raw string
    // parse results
    CHECK(*java::parseDate("1970-01-01T00:00:00", ISO, true) == 0);
    CHECK(*java::parseDate("1970-01-01T00:00:01", ISO, true) == 1000);
    CHECK(*java::parseDate("1969-12-31T23:59:59", ISO, true) == -1000);
    CHECK(*java::parseDate("2023-05-14 23:38:47.123", "yyyy-MM-dd HH:mm:ss.SSS", true) == 1684107527123LL);
    CHECK(*java::parseDate("2023-05-14 23:38:47.123", "yyyy-MM-dd HH:mm:ss.SSS", false) == 1684107527123LL - 7 * 3600000LL);
    // lenient rollover, as in Java
    CHECK(java::reformatDate("2023-13-01T00:00:00", ISO, true) == "1 Jan 2024 07:00:00");
    CHECK(java::reformatDate("2023-02-30T00:00:00", ISO, true) == "2 Mar 2023 07:00:00");
    CHECK(java::reformatDate("2023-05-14T24:00:00", ISO, true) == "15 May 2023 07:00:00");
    CHECK(java::reformatDate("2023-05-14T23:38:47 trailing text", ISO, true) == "15 May 2023 06:38:47");  // trailing text ignored
    // the d field is not padded, HH is
    CHECK(java::formatDate(1678602000000LL) == "12 Mar 2023 13:20:00");
    CHECK(java::formatDate(1672531200000LL) == "1 Jan 2023 07:00:00");
    // DST zone: overlap resolves to the later instant like Java (golden9 has the exhaustive version)
    setenv("TZ", "America/New_York", 1);
    tzset();
    CHECK(*java::parseDate("2023-11-05 01:30:00.000", "yyyy-MM-dd HH:mm:ss.SSS", false) == 1699165800000LL);
    setenv("TZ", "UTC", 1);
    tzset();
    // unsupported pattern letters throw instead of guessing
    bool a = false, b = false, c = false;
    try { java::parseDate("x", "d MMM yyyy", true); } catch (const std::invalid_argument&) { a = true; }
    try { java::parseDate("x", "yy", true); } catch (const std::invalid_argument&) { b = true; }
    try { java::parseDate("x", "yyyy 'open", true); } catch (const std::invalid_argument&) { c = true; }
    CHECK(a && b && c);
    // YaSOnline: formatDate(int seconds) = format(new Date(timestamp * 1000L))
    CHECK(java::formatDate(1431646727LL * 1000) == "14 May 2015 23:38:47");
    if (failures == 0) std::printf("batch22: all tests passed\n");
    return failures ? 1 : 0;
}
