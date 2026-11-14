#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include "opsu/downloads/Url.h"
#include "opsu/io/ReadableByteChannel.h"

namespace opsu {

/**
 * Seam for the HTTP client (still undecided: libcurl / Android HttpURLConnection via JNI / ...). Models exactly what
 * Download.java used of java.net.HttpURLConnection, so Download keeps the redirect and transfer logic itself:
 * the client must NOT follow redirects (`setInstanceFollowRedirects(false)`) and must not use caches.
 */
struct HttpRequest {
    Url url;
    int connectTimeoutMs = 5000;  // Download::CONNECTION_TIMEOUT
    int readTimeoutMs = 10000;    // Download::READ_TIMEOUT
    /** Request headers, "User-Agent" first, then the server's custom headers (which may replace it, case-insensitively). */
    std::map<std::string, std::string> headers;
    /** Java: Utils.setSSLCertValidation(false) around the connection (the client should skip certificate validation). */
    bool disableSSLCertValidation = false;
};

/** One response (HttpURLConnection after getResponseCode()). Destroying it = disconnect(). */
class HttpResponse {
public:
    virtual ~HttpResponse() = default;
    /** conn.getResponseCode(). */
    virtual int statusCode() const = 0;
    /** conn.getURL(): the URL this response came from. */
    virtual const Url& url() const = 0;
    /** conn.getHeaderField(name), case-insensitive; nullopt if absent. */
    virtual std::optional<std::string> header(const std::string& name) const = 0;
    /** conn.getContentLength(): -1 if unknown (or > INT_MAX). */
    virtual int contentLength() const = 0;
    /**
     * conn.getInputStream(): the body. Throws java::IOException for HTTP error statuses (>= 400), like HttpURLConnection.
     * `close()` on the returned channel must be safe to call from another thread (Download::cancel does) and must make a
     * blocked/following `read` fail with java::IOException.
     */
    virtual ReadableByteChannel& body() = 0;
};

class HttpClient {
public:
    virtual ~HttpClient() = default;
    /** Opens the connection and reads the status line / headers. Throws java::IOException on connect / read timeout etc. */
    virtual std::unique_ptr<HttpResponse> open(const HttpRequest& request) = 0;
};

}  // namespace opsu
