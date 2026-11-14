#include "opsu/downloads/Url.h"
#include <cctype>
#include <vector>

namespace opsu {

static bool knownProtocol(const std::string& p) {
    return p == "http" || p == "https" || p == "ftp" || p == "file" || p == "jar" || p == "jmod" || p == "jrt" || p == "mailto";
}

/** Index of the ':' ending a leading scheme (letters, digits, + - .), or npos. */
static size_t schemeEnd(const std::string& s) {
    if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0])))
        return std::string::npos;
    for (size_t i = 1; i < s.size(); i++) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == ':') return i;
        if (!(std::isalnum(c) || c == '+' || c == '-' || c == '.')) return std::string::npos;
    }
    return std::string::npos;
}

static std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void Url::setAuthority(const std::string& a) {
    authority = a;
    std::string hp = a;
    size_t at = hp.rfind('@');
    if (at != std::string::npos) hp = hp.substr(at + 1);
    host = hp;
    port = -1;
    size_t colon = hp.rfind(':');
    if (colon != std::string::npos && hp.find(']', colon) == std::string::npos) {
        std::string ps = hp.substr(colon + 1);
        host = hp.substr(0, colon);
        bool digits = !ps.empty() && ps.size() <= 9;
        for (char c : ps) if (!std::isdigit(static_cast<unsigned char>(c))) digits = false;
        if (digits) port = std::stoi(ps);
    }
}

/** Splits "<path>?<query>#<ref>" into the three parts. */
static void splitRest(const std::string& rest, std::string& path, std::string& query, bool& hasQuery, std::string& ref, bool& hasRef) {
    std::string r = rest;
    size_t h = r.find('#');
    hasRef = (h != std::string::npos);
    ref = hasRef ? r.substr(h + 1) : "";
    if (hasRef) r = r.substr(0, h);
    size_t q = r.find('?');
    hasQuery = (q != std::string::npos);
    query = hasQuery ? r.substr(q + 1) : "";
    path = hasQuery ? r.substr(0, q) : r;
}

Url Url::parse(const std::string& spec) {
    size_t e = schemeEnd(spec);
    if (e == std::string::npos)
        throw MalformedURLException("no protocol: " + spec);
    Url u;
    u.protocol = lower(spec.substr(0, e));
    if (!knownProtocol(u.protocol))
        throw MalformedURLException("unknown protocol: " + u.protocol);
    std::string rest = spec.substr(e + 1);
    if (rest.compare(0, 2, "//") == 0) {
        size_t end = rest.find_first_of("/?#", 2);
        std::string auth = rest.substr(2, end == std::string::npos ? std::string::npos : end - 2);
        u.setAuthority(auth);
        rest = (end == std::string::npos) ? "" : rest.substr(end);
    }
    splitRest(rest, u.path, u.query, u.hasQuery_, u.ref, u.hasRef_);
    return u;
}

std::string Url::toString() const {
    std::string s = protocol + ":";
    if (!authority.empty()) s += "//" + authority;
    s += path;
    if (hasQuery_) s += "?" + query;
    if (hasRef_) s += "#" + ref;
    return s;
}

/** Joins a base path's directory with a relative path and removes "." / ".." segments. */
static std::string joinPath(const std::string& basePath, const std::string& rel) {
    size_t slash = basePath.rfind('/');
    std::string full = (slash == std::string::npos ? std::string("/") : basePath.substr(0, slash + 1)) + rel;
    std::vector<std::string> out;
    size_t i = 0;
    bool trailing = false;
    while (i <= full.size()) {
        size_t j = full.find('/', i);
        std::string seg = full.substr(i, j == std::string::npos ? std::string::npos : j - i);
        bool last = (j == std::string::npos);
        if (seg == "..") {
            if (!out.empty() && !out.back().empty()) out.pop_back();
            trailing = last;
        } else if (seg == ".") {
            trailing = last;
        } else {
            out.push_back(seg);
            trailing = false;
        }
        if (last) break;
        i = j + 1;
    }
    std::string res;
    for (size_t k = 0; k < out.size(); k++) {
        if (k) res += '/';
        res += out[k];
    }
    if (trailing && (res.empty() || res.back() != '/')) res += '/';
    if (res.empty() || res[0] != '/') res = "/" + res;
    return res;
}

Url Url::resolve(const Url& context, const std::string& spec) {
    if (schemeEnd(spec) != std::string::npos)
        return parse(spec);
    Url u = context;
    if (spec.empty())
        return u;
    if (spec.compare(0, 2, "//") == 0)
        return parse(context.protocol + ":" + spec);
    u.hasRef_ = false; u.ref.clear();
    if (spec[0] == '#') {
        u.hasRef_ = true; u.ref = spec.substr(1);
        return u;
    }
    u.hasQuery_ = false; u.query.clear();
    if (spec[0] == '?') {
        std::string p, r; bool hq, hr;
        splitRest(spec, p, u.query, hq, r, hr);
        u.hasQuery_ = hq; u.hasRef_ = hr; u.ref = r;
        return u;
    }
    std::string path, query, ref; bool hq, hr;
    splitRest(spec, path, query, hq, ref, hr);
    u.path = (path[0] == '/') ? path : joinPath(context.path, path);
    u.query = query; u.hasQuery_ = hq; u.ref = ref; u.hasRef_ = hr;
    return u;
}

}  // namespace opsu
