#pragma once
#include <atomic>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "opsu/downloads/HttpClient.h"
#include "opsu/downloads/ReadableByteChannelWrapper.h"
#include "opsu/downloads/Url.h"

namespace opsu {

/**
 * File download. Port of itdelatrisu.opsu.downloads.Download: status machine, redirect rules, transfer loop, progress, speed / ETA.
 * The network itself is the `HttpClient` seam (HttpClient.h); everything else is here.
 *
 * Seams / deviations
 * - `File` = UTF-8 path string. `rename` is `std::optional<std::string>` (nullopt = null).
 * - `start()` returns `bool` (false = Java `null`: not WAITING, or a thread is already running) instead of the `Thread`. The Download
 *   OWNS its worker thread(s): `join()` waits for them and the destructor cancels a still-active download and joins (a std::thread
 *   must not be destroyed joinable). `cancel()` is asynchronous like Java (it spawns a thread); `join()` also waits for it.
 * - `status` / `contentLength` are atomic (the Java fields were plain and shared with the UI thread); the channel / file pointers are
 *   guarded by a mutex. The HTTP client is not owned (`setHttpClient`, must outlive the download); with none set `start()`'s thread
 *   fails like a connection error. The clock is injectable (`setClock`) for tests; default = system clock in ms.
 * - `ErrorHandler.error(msg, e, report)` is not ported: static `setErrorHandler` hook, default `Log::error`.
 * - Options.USER_AGENT is `Download::USER_AGENT`. Utils.setSSLCertValidation becomes `HttpRequest::disableSSLCertValidation`.
 * - Java's `transferFrom` loop spins forever when the server closes the stream early (it keeps getting 0 bytes), so the "N bytes expected,
 *   M received" branch that exists right after it was unreachable. Here the loop stops on end-of-stream, so a truncated download now
 *   ends as ERROR with that message (listener.error()), which is evidently what the Java code meant.
 * - Java quirks kept: an error inside the transfer loop calls listener.error() and then the status is overwritten to CANCELLED (so the
 *   final status of such a download is CANCELLED, not ERROR); COMPLETE is published before the final rename; 4 redirects are allowed
 *   (`redirectCount > MAX_REDIRECTS` is checked before incrementing); chunked / unknown length (-1) fails ("Negative content length");
 *   a cancel() that lands before the worker publishes DOWNLOADING is overwritten by it (race in the Java too).
 */
class Download {
public:
    /** Connection timeout, in ms. */
    static constexpr int CONNECTION_TIMEOUT = 5000;
    /** Read timeout, in ms. */
    static constexpr int READ_TIMEOUT = 10000;
    /** Maximum number of HTTP/HTTPS redirects to follow. */
    static constexpr int MAX_REDIRECTS = 3;
    /** Options.USER_AGENT. */
    static constexpr const char* USER_AGENT =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/63.0.3239.132 Safari/537.36";

    /** Download statuses. */
    enum class Status { WAITING, DOWNLOADING, COMPLETE, CANCELLED, ERROR };
    /** Status.getName(). */
    static const char* getName(Status s);

    /** Download.DownloadListener (empty std::function = no-op). */
    struct Listener {
        std::function<void()> completed;  // a download has completed
        std::function<void()> error;      // an error has occurred
    };

    /** (message, exception or nullptr, report) -> ErrorHandler.error. Default: Log::error. Pass an empty function to restore it. */
    using ErrorHandlerFn = std::function<void(const std::string&, const std::exception*, bool)>;
    static void setErrorHandler(ErrorHandlerFn fn);
    /** ErrorHandler.error(msg, e, report): calls the handler set above (default Log::error). `e` may be nullptr. Thread-safe. */
    static void reportError(const std::string& msg, const std::exception* e, bool report);

    /**
     * @param remoteURL the download URL (a malformed one sets status ERROR and calls the error handler, like Java)
     * @param localPath the path to save the download
     * @param rename the file name to rename the download to when complete (passed through cleanFileName(.., '-'))
     */
    Download(const std::string& remoteURL, const std::string& localPath, std::optional<std::string> rename = std::nullopt);
    ~Download();
    Download(const Download&) = delete;
    Download& operator=(const Download&) = delete;

    const std::optional<Url>& getRemoteURL() const { return url; }
    void setListener(Listener l) { listener = std::move(l); }
    void setRequestHeaders(std::map<std::string, std::string> headers) { requestHeaders = std::move(headers); }
    void setSSLCertValidation(bool enabled) { disableSSLCertValidation = !enabled; }
    void setHttpClient(HttpClient* c) { client = c; }
    void setClock(std::function<int64_t()> c) { clock = std::move(c); }

    /** Starts the download from the "waiting" status. Returns false if none started. */
    bool start();
    /** Waits for the worker thread and any cancel threads. */
    void join();

    Status getStatus() const { return status.load(); }
    /** Returns true if transfers are currently taking place. */
    bool isTransferring() const;
    /** Returns true if the download is active (WAITING or DOWNLOADING). */
    bool isActive() const { Status s = status.load(); return s == Status::WAITING || s == Status::DOWNLOADING; }
    /** Size of the download content in bytes, or -1 if not calculated (or an error occurred). */
    int contentLength() const { return contentLength_.load(); }
    /** Completion percentage, or -1 for ERROR / CANCELLED. */
    float getProgress() const;
    /** Number of bytes read so far. */
    long long readSoFar() const;
    /** Last calculated download speed ("1.5 MB/s"), nullopt before the first calculation. */
    std::optional<std::string> getDownloadSpeed();
    /** Last calculated ETA ("1m5s"), nullopt before the first calculation. */
    std::optional<std::string> getTimeRemaining();
    /** Cancels the download, if running (asynchronous). */
    void cancel();

private:
    void run();
    long long transferFrom(ReadableByteChannel& in, long long count, bool& eof);
    void updateReadSoFar();
    void closeStreams();

    std::string localFile;
    std::optional<std::string> rename;
    std::optional<Url> url;
    Listener listener;
    std::map<std::string, std::string> requestHeaders;
    bool disableSSLCertValidation = false;
    HttpClient* client = nullptr;
    std::function<int64_t()> clock;

    mutable std::mutex m;  // guards response, rbc, fos
    std::unique_ptr<HttpResponse> response;
    std::unique_ptr<ReadableByteChannelWrapper> rbc;
    std::unique_ptr<std::ofstream> fos;

    std::atomic<int> contentLength_{-1};
    std::atomic<Status> status{Status::WAITING};

    std::mutex speedMutex;  // guards the speed / ETA fields
    int64_t lastReadSoFarTime = -1;
    long long lastReadSoFar = -1;
    std::optional<std::string> lastDownloadSpeed, lastTimeRemaining;
    long long avgDownloadSpeed = 0;

    std::mutex threadMutex;
    std::thread dlThread;
    std::vector<std::thread> cancelThreads;

    static constexpr int UPDATE_INTERVAL = 1000;
    static constexpr double DOWNLOAD_SPEED_SMOOTHING = 0.25;
};

}  // namespace opsu
