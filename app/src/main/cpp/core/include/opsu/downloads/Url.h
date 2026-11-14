#pragma once
#include <stdexcept>
#include "opsu/util/JavaIO.h"
#include <string>

namespace opsu {

/** java.net.MalformedURLException. */
struct MalformedURLException : java::IOException {  // Java: extends IOException (Download catches it as one)
    using java::IOException::IOException;
};

/**
 * Small stand-in for java.net.URL (only what Download needs): parse, accessors, toString and `resolve` (= `new URL(context, spec)`).
 *
 * Deliberately a SUBSET of Java's parser:
 * - protocols accepted (lower-cased, like Java): http, https, ftp, file, jar, jmod, jrt, mailto. Anything else, or no scheme, throws
 *   MalformedURLException (Java: "unknown protocol" / "no protocol").
 * - `//authority` is split at the first '/', '?' or '#'; host / port are derived from it; the path, query and fragment follow.
 *   No percent-decoding, no IPv6 / userinfo validation (a bad port just means getPort() == -1).
 * - `resolve` handles: absolute URL, `//host/...`, `/path`, `?query`, `#ref`, relative path (with `.` and `..` segments) and "" (the
 *   context). Java's odd "same-protocol relative" form (`http:foo`) is treated as an absolute spec.
 */
class Url {
public:
    static Url parse(const std::string& spec);                       // throws MalformedURLException
    static Url resolve(const Url& context, const std::string& spec);  // throws MalformedURLException

    const std::string& getProtocol() const { return protocol; }
    const std::string& getAuthority() const { return authority; }
    const std::string& getHost() const { return host; }
    int getPort() const { return port; }                  // -1 if none
    const std::string& getPath() const { return path; }
    const std::string& getQuery() const { return query; }  // without '?'; "" if none
    const std::string& getRef() const { return ref; }      // without '#'; "" if none
    bool hasQuery() const { return hasQuery_; }
    /** URL.toString() / toExternalForm(). */
    std::string toString() const;

private:
    std::string protocol, authority, host, path, query, ref;
    int port = -1;
    bool hasQuery_ = false, hasRef_ = false;
    void setAuthority(const std::string& a);
};

}  // namespace opsu
