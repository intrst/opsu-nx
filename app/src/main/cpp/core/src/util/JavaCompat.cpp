#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"
#include <charconv>
#include <climits>
#include <cmath>
#include <limits>
#include <type_traits>
#include <cstdio>
#include <cstdlib>

namespace opsu {
namespace java {

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    if (s.find(delim) == std::string::npos) { out.push_back(s); return out; }
    size_t start = 0;
    for (;;) {
        size_t p = s.find(delim, start);
        if (p == std::string::npos) { out.push_back(s.substr(start)); break; }
        out.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

std::vector<std::string> splitAll(const std::string& s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        size_t p = s.find(delim, start);
        if (p == std::string::npos) { out.push_back(s.substr(start)); break; }
        out.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    return out;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && static_cast<unsigned char>(s[b]) <= ' ') b++;
    while (e > b && static_cast<unsigned char>(s[e - 1]) <= ' ') e--;
    return s.substr(b, e - b);
}

static long long parseIntegral(const std::string& s, long long lo, long long hi) {
    size_t i = 0;
    bool neg = false;
    if (s.empty()) throw NumberFormatException("For input string: \"" + s + "\"");
    if (s[0] == '-' || s[0] == '+') { neg = s[0] == '-'; i = 1; }
    if (i == s.size()) throw NumberFormatException("For input string: \"" + s + "\"");
    long long v = 0;
    for (; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') throw NumberFormatException("For input string: \"" + s + "\"");
        v = v * 10 + (s[i] - '0');
        if (v > (1LL << 40)) throw NumberFormatException("For input string: \"" + s + "\"");
    }
    if (neg) v = -v;
    if (v < lo || v > hi) throw NumberFormatException("Value out of range. Value:\"" + s + "\"");
    return v;
}

int parseInt(const std::string& s) { return static_cast<int>(parseIntegral(s, INT_MIN, INT_MAX)); }
int8_t parseByte(const std::string& s) { return static_cast<int8_t>(parseIntegral(s, -128, 127)); }
int16_t parseShort(const std::string& s) { return static_cast<int16_t>(parseIntegral(s, -32768, 32767)); }

namespace {
template <class T>
T parseFp(const std::string& in) {
    size_t b = 0, e = in.size();
    while (b < e && static_cast<unsigned char>(in[b]) <= ' ') b++;
    while (e > b && static_cast<unsigned char>(in[e - 1]) <= ' ') e--;
    std::string s = in.substr(b, e - b);
    auto bad = [&]() { return NumberFormatException(s.empty() ? "empty String" : "For input string: \"" + s + "\""); };
    if (s.empty()) throw bad();
    size_t i = 0;
    bool neg = false;
    if (s[0] == '-' || s[0] == '+') { neg = s[0] == '-'; i = 1; }
    std::string body = s.substr(i);
    if (body == "NaN") return std::numeric_limits<T>::quiet_NaN();
    if (body == "Infinity") return neg ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
    if (!body.empty() && std::string("fFdD").find(body.back()) != std::string::npos) body.pop_back();
    size_t j = 0, digits = 0;
    while (j < body.size() && body[j] >= '0' && body[j] <= '9') { j++; digits++; }
    if (j < body.size() && body[j] == '.') {
        j++;
        while (j < body.size() && body[j] >= '0' && body[j] <= '9') { j++; digits++; }
    }
    if (digits == 0) throw bad();
    if (j < body.size() && (body[j] == 'e' || body[j] == 'E')) {
        j++;
        if (j < body.size() && (body[j] == '+' || body[j] == '-')) j++;
        size_t ed = 0;
        while (j < body.size() && body[j] >= '0' && body[j] <= '9') { j++; ed++; }
        if (ed == 0) throw bad();
    }
    if (j != body.size()) throw bad();
    // `body` is plain decimal text here (validated above), so strtof / strtod parse exactly it. Both round correctly
    // with ONE rounding (strtof for float, so no double rounding). Not std::from_chars: libc++ in older Android NDKs
    // has no floating-point from_chars. Out of range gives +-inf / 0 like Java (ERANGE is ignored). Needs the "C" locale
    // (the default; the core never calls setlocale).
    T v;
    if constexpr (std::is_same<T, float>::value) v = std::strtof(body.c_str(), nullptr);
    else v = static_cast<T>(std::strtod(body.c_str(), nullptr));
    return neg ? -v : v;
}
}  // namespace

float parseFloat(const std::string& in) { return parseFp<float>(in); }
double parseDouble(const std::string& in) { return parseFp<double>(in); }

int toInt(double v) {
    if (std::isnan(v)) return 0;
    if (v >= static_cast<double>(INT_MAX)) return INT_MAX;
    if (v <= static_cast<double>(INT_MIN)) return INT_MIN;
    return static_cast<int>(v);
}

namespace {

/** Shortest round-trip decimal digits of v>0: value = 0.d1d2d3... * 10^exp10. */
void shortestDigits(double v, std::string& digits, int& exp10) {
    char buf[64];
    auto r = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::scientific);
    std::string s(buf, r.ptr);  // d.ddddde+XX
    size_t epos = s.find('e');
    std::string mant = s.substr(0, epos);
    int e = std::atoi(s.c_str() + epos + 1);
    digits.clear();
    for (char c : mant) if (c != '.') digits.push_back(c);
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    exp10 = e + 1;
}

/** Keep the first n digits, rounding half up (on the digit string). May bump exp10. */
void roundHalfUp(std::string& d, int& exp10, int n) {
    if (n < 0) { d = "0"; return; }
    if (static_cast<int>(d.size()) <= n) return;
    bool up = d[n] >= '5';
    d.resize(n);
    if (up) {
        int i = n - 1;
        while (i >= 0 && d[i] == '9') { d[i] = '0'; i--; }
        if (i >= 0) d[i]++;
        else { d.insert(d.begin(), '1'); exp10++; if (n > 0) d.resize(n); else d.clear(); }
    }
    if (d.empty()) d = "0";
}

}  // namespace

std::string formatFixed(double v, int prec) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v < 0 ? "-Infinity" : "Infinity";
    bool neg = std::signbit(v);
    double a = std::fabs(v);
    std::string intPart = "0", frac(prec, '0');
    if (a != 0.0) {
        std::string d; int e;
        shortestDigits(a, d, e);
        // number of digits kept = digits before point + prec
        int keep = e + prec;
        if (keep < 0) { d = "0"; e = 1; }
        else if (keep == 0) {
            // everything lies right of the rounding position: round on the first digit
            if (d[0] >= '5') { d = "1"; e = e + 1; } else { d = "0"; e = 1; }
        } else roundHalfUp(d, e, keep);
        bool zero = d.find_first_not_of('0') == std::string::npos;
        if (zero) { d = "0"; e = 1; }
        std::string all;
        if (e <= 0) { all = std::string(-e, '0') + d; e = 0; } else all = d;
        while (static_cast<int>(all.size()) < e + prec) all.push_back('0');
        intPart = e > 0 ? all.substr(0, e) : "0";
        frac = all.substr(e, prec);
        frac.resize(prec, '0');
    }
    std::string out = (neg ? "-" : "") + intPart;
    if (prec > 0) out += "." + frac;
    return out;
}

std::string formatGeneral(double v) {
    const int prec = 6;
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v < 0 ? "-Infinity" : "Infinity";
    bool neg = std::signbit(v);
    double a = std::fabs(v);
    std::string out = neg ? "-" : "";
    if (a == 0.0) return out + "0.00000";
    std::string d; int e;
    shortestDigits(a, d, e);
    roundHalfUp(d, e, prec);
    int expRounded = e - 1;
    while (static_cast<int>(d.size()) < prec) d.push_back('0');
    if (expRounded >= -4 && expRounded < prec) {
        int decimals = prec - expRounded - 1;
        std::string s;
        if (expRounded >= 0) {
            s = d.substr(0, expRounded + 1);
            if (decimals > 0) s += "." + d.substr(expRounded + 1, decimals);
        } else {
            s = "0." + std::string(-expRounded - 1, '0') + d.substr(0, prec);
            s.resize(2 + decimals, '0');
        }
        return out + s;
    }
    char eb[16];
    std::snprintf(eb, sizeof eb, "e%c%02d", expRounded < 0 ? '-' : '+', std::abs(expRounded));
    return out + d.substr(0, 1) + "." + d.substr(1, prec - 1) + eb;
}

namespace {
template <class T>
std::string fpToString(T f) {
    if (std::isnan(f)) return "NaN";
    if (std::isinf(f)) return f < 0 ? "-Infinity" : "Infinity";
    bool neg = std::signbit(f);
    T a = std::fabs(f);
    std::string out = neg ? "-" : "";
    if (a == T(0)) return out + "0.0";
    char buf[64];
    auto r = std::to_chars(buf, buf + sizeof buf, a, std::chars_format::scientific);
    std::string s(buf, r.ptr);
    size_t epos = s.find('e');
    int e = std::atoi(s.c_str() + epos + 1);
    std::string d;
    for (char c : s.substr(0, epos)) if (c != '.') d.push_back(c);
    while (d.size() > 1 && d.back() == '0') d.pop_back();
    if (d.size() == 1) {
        // JDK 19+: when the shortest decimal has 1 digit, the closest 2-digit decimal is chosen
        auto r2 = std::to_chars(buf, buf + sizeof buf, a, std::chars_format::scientific, 1);
        std::string s2(buf, r2.ptr);
        size_t ep2 = s2.find('e');
        e = std::atoi(s2.c_str() + ep2 + 1);
        d.clear();
        for (char c : s2.substr(0, ep2)) if (c != '.') d.push_back(c);
        while (d.size() > 1 && d.back() == '0') d.pop_back();
    }
    if (a >= static_cast<T>(1e-3) && a < static_cast<T>(1e7)) {
        std::string ip, fp;
        if (e >= 0) {
            while (static_cast<int>(d.size()) < e + 1) d.push_back('0');
            ip = d.substr(0, e + 1);
            fp = d.substr(e + 1);
        } else {
            ip = "0";
            fp = std::string(-e - 1, '0') + d;
        }
        if (fp.empty()) fp = "0";
        return out + ip + "." + fp;
    }
    std::string m = d.substr(0, 1) + "." + (d.size() > 1 ? d.substr(1) : "0");
    return out + m + "E" + std::to_string(e);
}
}  // namespace

std::string floatToString(float f) { return fpToString<float>(f); }
std::string doubleToString(double d) { return fpToString<double>(d); }

std::string formatTrimmed(double v, int maxFrac) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v < 0 ? "-\xE2\x88\x9E" : "\xE2\x88\x9E";
    char buf[512];
    std::string s;
    if (std::fabs(v) >= 9007199254740992.0) {  // >= 2^53: no fraction; Java pads the shortest digits with zeros
        std::string d; int e;
        shortestDigits(std::fabs(v), d, e);
        s = d + std::string(static_cast<size_t>(e) - d.size(), '0');
    } else {
        std::snprintf(buf, sizeof buf, "%.*f", maxFrac, std::fabs(v));  // exact value, ties to even
        s = buf;
    }
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return (std::signbit(v) ? "-" : "") + s;
}

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    const std::string u = decodeUtf8Lossy(s);
    std::string out;
    out.reserve(u.size());
    for (unsigned char c : u) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '*' || c == '_') {
            out += static_cast<char>(c);
        } else if (c == ' ') {
            out += '+';
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

}  // namespace java
}  // namespace opsu
