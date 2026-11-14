#include "opsu/util/JavaDate.h"
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <vector>

namespace opsu {
namespace java {

namespace {

struct Elem {
    bool field;      // true: numeric field of `letter`; false: literal `ch`
    char letter;
    int count;
    char ch;
};

std::vector<Elem> compilePattern(const std::string& p) {
    std::vector<Elem> out;
    for (size_t i = 0; i < p.size();) {
        char c = p[i];
        if (c == '\'') {  // 'quoted' text; '' is a literal quote
            if (i + 1 < p.size() && p[i + 1] == '\'') { out.push_back({false, 0, 0, '\''}); i += 2; continue; }
            size_t j = i + 1;
            while (j < p.size()) {
                if (p[j] == '\'') {
                    if (j + 1 < p.size() && p[j + 1] == '\'') { out.push_back({false, 0, 0, '\''}); j += 2; continue; }
                    break;
                }
                out.push_back({false, 0, 0, p[j]});
                j++;
            }
            if (j >= p.size()) throw std::invalid_argument("Unterminated quote in pattern");
            i = j + 1;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            size_t j = i;
            while (j < p.size() && p[j] == c) j++;
            int n = static_cast<int>(j - i);
            bool ok = (c == 'y' && n >= 3) || ((c == 'M' || c == 'd' || c == 'H' || c == 'm' || c == 's' || c == 'S') && n <= 2) ||
                      (c == 'S' && n == 3);
            if (!ok) throw std::invalid_argument(std::string("Pattern letter not supported: ") + c);
            out.push_back({true, c, n, 0});
            i = j;
        } else {
            out.push_back({false, 0, 0, c});
            i++;
        }
    }
    return out;
}

int64_t floorDiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

// days since 1970-01-01 of a proleptic Gregorian date (month 1..12); Howard Hinnant's algorithm
int64_t daysFromCivil(int64_t y, int m, int d) {
    y -= m <= 2;
    int64_t era = floorDiv(y, 400);
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

}  // namespace

std::optional<int64_t> parseDate(const std::string& text, const std::string& pattern, bool utc) {
    const std::vector<Elem> elems = compilePattern(pattern);
    int64_t year = 1970, month0 = 0, day = 1, hour = 0, minute = 0, second = 0, milli = 0;  // Calendar defaults (1970-01-01 00:00:00.000)
    size_t pos = 0;
    for (const Elem& e : elems) {
        if (!e.field) {
            if (pos >= text.size() || text[pos] != e.ch) return std::nullopt;
            pos++;
            continue;
        }
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) pos++;  // subParse skips blanks
        if (pos >= text.size()) return std::nullopt;
        bool neg = false;
        if (text[pos] == '-') { neg = true; pos++; }
        size_t start = pos;
        int64_t v = 0;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            if (pos - start >= 9) return std::nullopt;  // > 9 digits: Java wraps through Long.intValue; not supported (see header)
            v = v * 10 + (text[pos] - '0');
            pos++;
        }
        if (pos == start) return std::nullopt;
        if (neg) v = -v;
        switch (e.letter) {
            case 'y': year = v; break;
            case 'M': month0 = v - 1; break;
            case 'd': day = v; break;
            case 'H': hour = v; break;
            case 'm': minute = v; break;
            case 's': second = v; break;
            case 'S': milli = v; break;
        }
    }
    // GregorianCalendar.computeTime (lenient): normalise the month into the year, then add (day - 1) days and the time of day
    year += floorDiv(month0, 12);
    int mon = static_cast<int>(month0 - floorDiv(month0, 12) * 12) + 1;
    int64_t days = daysFromCivil(year, mon, 1) + (day - 1);
    int64_t timeOfDay = ((hour * 60 + minute) * 60 + second) * 1000 + milli;
    if (utc) return days * 86400000LL + timeOfDay;
    // default zone: let libc resolve the wall time (DST gap / overlap rules are libc's, see PORTING_NOTES batch 22)
    int64_t wallSec = floorDiv(days * 86400000LL + timeOfDay, 1000);
    int64_t msPart = (days * 86400000LL + timeOfDay) - wallSec * 1000;
    int64_t d2 = floorDiv(wallSec, 86400);
    int64_t secOfDay = wallSec - d2 * 86400;
    // civil date from d2
    int64_t z = d2 + 719468, era = floorDiv(z, 146097), doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, yy = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    int dd = static_cast<int>(doy - (153 * mp + 2) / 5 + 1), mm = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    yy += mm <= 2;
    std::tm tmv{};
    tmv.tm_year = static_cast<int>(yy - 1900);
    tmv.tm_mon = mm - 1;
    tmv.tm_mday = dd;
    tmv.tm_hour = static_cast<int>(secOfDay / 3600);
    tmv.tm_min = static_cast<int>(secOfDay % 3600 / 60);
    tmv.tm_sec = static_cast<int>(secOfDay % 60);
    tmv.tm_isdst = -1;
    tzset();
    std::time_t t = std::mktime(&tmv);  // resolves a wall time inside a DST gap the way Java does (verified by golden9)
    if (t == static_cast<std::time_t>(-1)) return std::nullopt;
    // A wall time that occurs twice (DST overlap): glibc picks the first (daylight) instant, Java the later one (standard time).
    // Try the UTC offsets in force a day before / after and keep the latest instant whose wall clock reads exactly the requested time.
    std::time_t best = t;
    bool haveValid = false;
    for (int dir = -1; dir <= 1; dir += 2) {
        std::time_t probe = t + dir * 86400;
        std::tm pt{};
        if (!localtime_r(&probe, &pt)) continue;
        std::time_t cand = static_cast<std::time_t>(wallSec - pt.tm_gmtoff);
        std::tm ct{};
        if (!localtime_r(&cand, &ct)) continue;
        if (static_cast<int64_t>(cand) + ct.tm_gmtoff == wallSec && (!haveValid || cand > best)) { best = cand; haveValid = true; }
    }
    if (haveValid) t = best;
    return static_cast<int64_t>(t) * 1000 + msPart;
}

std::string formatDate(int64_t epochMillis) {
    static const char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    tzset();
    std::time_t t = static_cast<std::time_t>(floorDiv(epochMillis, 1000));
    std::tm tmv{};
    if (!localtime_r(&t, &tmv)) return "";
    long long year = static_cast<long long>(tmv.tm_year) + 1900;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d %s %04lld %02d:%02d:%02d", tmv.tm_mday, months[tmv.tm_mon], year, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return buf;
}

std::string reformatDate(const std::string& s, const std::string& pattern, bool utc) {
    std::optional<int64_t> ms = parseDate(s, pattern, utc);
    return ms ? formatDate(*ms) : s;
}

}  // namespace java
}  // namespace opsu
