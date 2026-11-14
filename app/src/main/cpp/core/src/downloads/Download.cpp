#include "opsu/downloads/Download.h"
#include <chrono>
#include <filesystem>
#include "opsu/Utils.h"
#include "opsu/logging/Log.h"
#include "opsu/util/JavaIO.h"

namespace opsu {
namespace fs = std::filesystem;

static std::mutex& handlerMutex() { static std::mutex mu; return mu; }
static Download::ErrorHandlerFn& handlerRef() { static Download::ErrorHandlerFn fn; return fn; }

void Download::setErrorHandler(ErrorHandlerFn fn) {
    std::lock_guard<std::mutex> g(handlerMutex());
    handlerRef() = std::move(fn);
}

void Download::reportError(const std::string& msg, const std::exception* e, bool report) {
    Download::ErrorHandlerFn fn;
    { std::lock_guard<std::mutex> g(handlerMutex()); fn = handlerRef(); }
    if (fn) { fn(msg, e, report); return; }
    if (e) Log::error(msg, *e); else Log::error(msg);
}

const char* Download::getName(Status s) {
    switch (s) {
    case Status::WAITING: return "Waiting";
    case Status::DOWNLOADING: return "Downloading";
    case Status::COMPLETE: return "Complete";
    case Status::CANCELLED: return "Cancelled";
    case Status::ERROR: return "Error";
    }
    return "";
}

Download::Download(const std::string& remoteURL, const std::string& localPath, std::optional<std::string> rename_) {
    clock = [] {
        return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    };
    try {
        url = Url::parse(remoteURL);
    } catch (const MalformedURLException& e) {
        status = Status::ERROR;
        reportError("Bad download URL: '" + remoteURL + "'", &e, true);
        return;
    }
    localFile = localPath;
    if (rename_) rename = Utils::cleanFileName(*rename_, '-');
}

Download::~Download() {
    if (isActive())
        status = Status::CANCELLED;  // tell the worker to stop (the Java object would simply keep running)
    closeStreams();
    join();
}

void Download::closeStreams() {
    std::lock_guard<std::mutex> g(m);
    if (rbc && rbc->isOpen()) rbc->close();
    if (fos && fos->is_open()) fos->close();
}

void Download::join() {
    std::thread t;
    std::vector<std::thread> cts;
    {
        std::lock_guard<std::mutex> g(threadMutex);
        t = std::move(dlThread);
        cts = std::move(cancelThreads);
        cancelThreads.clear();
    }
    if (t.joinable()) t.join();
    for (auto& c : cts) if (c.joinable()) c.join();
}

bool Download::start() {
    if (status.load() != Status::WAITING)
        return false;
    std::lock_guard<std::mutex> g(threadMutex);
    if (dlThread.joinable())
        return false;  // already started (Java would have started a second thread)
    dlThread = std::thread([this] { run(); });
    return true;
}

long long Download::transferFrom(ReadableByteChannel& in, long long count, bool& eof) {
    uint8_t buf[8192];
    long long tw = 0;
    eof = false;
    while (tw < count) {
        size_t want = static_cast<size_t>(std::min<long long>(count - tw, sizeof buf));
        int nr = in.read(buf, want);
        if (nr <= 0) { eof = (nr < 0); break; }
        {
            std::lock_guard<std::mutex> g(m);
            if (!fos || !fos->is_open())
                throw java::IOException("ClosedChannelException");
            fos->write(reinterpret_cast<const char*>(buf), nr);
            if (!*fos) throw java::IOException("write failed");
        }
        tw += nr;
    }
    return tw;
}

void Download::run() {
    // ---- open connection (follow redirects by hand) ----
    std::unique_ptr<HttpResponse> resp;
    try {
        Url downloadURL = *url;
        int redirectCount = 0;
        bool isRedirect = false;
        do {
            isRedirect = false;
            if (!client) throw java::IOException("no HTTP client");
            HttpRequest req;
            req.url = downloadURL;
            req.connectTimeoutMs = CONNECTION_TIMEOUT;
            req.readTimeoutMs = READ_TIMEOUT;
            req.disableSSLCertValidation = disableSSLCertValidation;
            req.headers["User-Agent"] = USER_AGENT;
            for (const auto& kv : requestHeaders) {  // setRequestProperty replaces an existing header (case-insensitive name)
                for (auto it = req.headers.begin(); it != req.headers.end();) {
                    const std::string &a = it->first, &b = kv.first;
                    bool same = a.size() == b.size();
                    for (size_t i = 0; same && i < a.size(); i++)
                        same = std::tolower(static_cast<unsigned char>(a[i])) == std::tolower(static_cast<unsigned char>(b[i]));
                    it = same ? req.headers.erase(it) : std::next(it);
                }
                req.headers[kv.first] = kv.second;
            }
            resp = client->open(req);

            int st = resp->statusCode();
            if (st == 302 || st == 301 || st == 303 || st == 305) {  // MOVED_TEMP, MOVED_PERM, SEE_OTHER, USE_PROXY
                Url base = resp->url();
                std::optional<std::string> location = resp->header("Location");
                std::optional<Url> target;
                if (location)
                    target = Url::resolve(base, *location);
                resp.reset();  // disconnect

                std::string error;
                if (!location)
                    error = "Download for URL '" + base.toString() + "' is attempting to redirect without a 'location' header.";
                else if (target->getProtocol() != "http" && target->getProtocol() != "https")
                    error = "Download for URL '" + base.toString() + "' is attempting to redirect to a non-HTTP/HTTPS protocol '" +
                            target->getProtocol() + "'.";
                else if (redirectCount > MAX_REDIRECTS)
                    error = "Download for URL '" + base.toString() + "' is attempting too many redirects (over " +
                            std::to_string(MAX_REDIRECTS) + ").";
                if (!error.empty()) {
                    reportError(error, nullptr, false);
                    throw java::IOException("");
                }
                downloadURL = *target;
                redirectCount++;
                isRedirect = true;
            }
        } while (isRedirect);

        contentLength_ = resp->contentLength();
        if (contentLength_ < 0)
            throw java::IOException("Negative content length");
    } catch (const std::exception& e) {
        status = Status::ERROR;
        Log::warn("Failed to open connection.", e);
        if (listener.error) listener.error();
        return;
    }

    // ---- download file ----
    try {
        ReadableByteChannel& in = resp->body();  // throws for HTTP error statuses
        auto sink = std::make_unique<std::ofstream>(fs::u8path(localFile), std::ios::binary | std::ios::trunc);
        struct Guard {  // try-with-resources: close channel and file on every exit
            Download& d; ReadableByteChannel& in;
            ~Guard() { if (in.isOpen()) in.close(); std::lock_guard<std::mutex> g(d.m); if (d.fos && d.fos->is_open()) d.fos->close(); }
        };
        if (!*sink) {
            in.close();
            throw java::FileNotFoundException(localFile);
        }
        {
            std::lock_guard<std::mutex> g(m);
            response = std::move(resp);
            rbc = std::make_unique<ReadableByteChannelWrapper>(in);
            fos = std::move(sink);
        }
        Guard guard{*this, in};
        const int total = contentLength_.load();
        status = Status::DOWNLOADING;
        updateReadSoFar();
        long long bytesRead = 0;

        try {
            while (status.load() == Status::DOWNLOADING && bytesRead < total) {
                bool eof = false;
                long long readed = transferFrom(*rbc, std::min<long long>(8192, total - bytesRead), eof);
                bytesRead += readed;
                if (readed == 0 && eof) break;  // deviation: Java spins here (see header)
            }
        } catch (const std::exception&) {
            if (status.load() != Status::CANCELLED) {
                status = Status::ERROR;
                Log::warn("Error while downloading.");
                if (listener.error) listener.error();
            }
        }

        if (status.load() == Status::DOWNLOADING) {
            if (bytesRead < total) {  // the entire file was not received
                status = Status::ERROR;
                Log::warn("Download '" + url->toString() + "' failed: " + std::to_string(total) + " bytes expected, " +
                          std::to_string(bytesRead) + " bytes received.");
                if (listener.error) listener.error();
                return;
            }
            // mark download as complete
            status = Status::COMPLETE;
            rbc->close();
            {
                std::lock_guard<std::mutex> g(m);
                if (fos && fos->is_open()) fos->close();
            }
            if (rename) {
                fs::path parent = fs::u8path(localFile).parent_path();
                Utils::moveFile(localFile, (parent / fs::u8path(*rename)).u8string());
            }
            if (listener.completed) listener.completed();
        } else {
            status = Status::CANCELLED;
        }
    } catch (const std::exception& e) {
        status = Status::ERROR;
        Log::warn("Failed to start download.", e);
        if (listener.error) listener.error();
    }
}

bool Download::isTransferring() const {
    std::lock_guard<std::mutex> g(m);
    return rbc && rbc->isOpen() && fos && fos->is_open();
}

float Download::getProgress() const {
    switch (status.load()) {
    case Status::WAITING: return 0.f;
    case Status::COMPLETE: return 100.f;
    case Status::DOWNLOADING: {
        std::lock_guard<std::mutex> g(m);
        int cl = contentLength_.load();
        if (rbc && fos && cl > 0)
            return static_cast<float>(rbc->getReadSoFar()) / static_cast<float>(cl) * 100.f;
        return 0.f;
    }
    case Status::CANCELLED:
    case Status::ERROR:
    default: return -1.f;
    }
}

long long Download::readSoFar() const {
    switch (status.load()) {
    case Status::COMPLETE: {
        std::lock_guard<std::mutex> g(m);
        return rbc ? rbc->getReadSoFar() : contentLength_.load();
    }
    case Status::DOWNLOADING: {
        std::lock_guard<std::mutex> g(m);
        return rbc ? rbc->getReadSoFar() : 0;  // Java falls through to 0
    }
    default: return 0;
    }
}

std::optional<std::string> Download::getDownloadSpeed() {
    updateReadSoFar();
    std::lock_guard<std::mutex> g(speedMutex);
    return lastDownloadSpeed;
}

std::optional<std::string> Download::getTimeRemaining() {
    updateReadSoFar();
    std::lock_guard<std::mutex> g(speedMutex);
    return lastTimeRemaining;
}

void Download::updateReadSoFar() {
    std::lock_guard<std::mutex> g(speedMutex);
    // only update while downloading
    if (status.load() != Status::DOWNLOADING) {
        lastDownloadSpeed = "";
        lastTimeRemaining = "";
        avgDownloadSpeed = 0;
        return;
    }

    // update download speed and ETA
    if (clock() > lastReadSoFarTime + UPDATE_INTERVAL) {
        long long rsf = readSoFar();
        int64_t now = clock();
        long long dlspeed = (rsf - lastReadSoFar) * 1000 / (now - lastReadSoFarTime);
        if (dlspeed > 0) {
            avgDownloadSpeed = (avgDownloadSpeed == 0) ? dlspeed
                : static_cast<long long>(DOWNLOAD_SPEED_SMOOTHING * static_cast<double>(dlspeed) +
                                         (1 - DOWNLOAD_SPEED_SMOOTHING) * static_cast<double>(avgDownloadSpeed));
            lastDownloadSpeed = Utils::bytesToString(avgDownloadSpeed) + "/s";
            long long t = (contentLength_.load() - rsf) / avgDownloadSpeed;
            if (t >= 3600)
                lastTimeRemaining = std::to_string(t / 3600) + "h" + std::to_string((t / 60) % 60) + "m" + std::to_string(t % 60) + "s";
            else
                lastTimeRemaining = std::to_string(t / 60) + "m" + std::to_string(t % 60) + "s";
        } else {
            avgDownloadSpeed = 0;
            lastDownloadSpeed = Utils::bytesToString(0) + "/s";
            lastTimeRemaining = "?";
        }
        lastReadSoFarTime = now;
        lastReadSoFar = rsf;
    }
    // first call
    else if (lastReadSoFarTime <= 0) {
        lastReadSoFar = readSoFar();
        lastReadSoFarTime = clock();
    }
}

void Download::cancel() {
    std::lock_guard<std::mutex> g(threadMutex);
    cancelThreads.emplace_back([this] {
        try {
            status = Status::CANCELLED;
            bool transferring = isTransferring();
            {
                std::lock_guard<std::mutex> lk(m);
                if (rbc && rbc->isOpen()) rbc->close();
                if (fos && fos->is_open()) fos->close();
            }
            if (transferring) {
                std::error_code ec;
                fs::path f = fs::u8path(localFile);
                if (fs::is_regular_file(f, ec))
                    fs::remove(f, ec);
            }
        } catch (const std::exception& e) {
            status = Status::ERROR;
            reportError("Failed to cancel download.", &e, true);
        }
    });
}

}  // namespace opsu
