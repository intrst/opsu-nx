// Tests for batch 17 (Url, Download, DownloadNode data half, DownloadList). Behavioural, no JVM golden (the box can't reach a server and
// the logic is state / string handling). The HTTP client and the byte channels are scripted fakes.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include "opsu/Utils.h"
#include "opsu/downloads/DownloadList.h"
#include "opsu/downloads/DownloadNode.h"
#include "opsu/downloads/Url.h"
#include "opsu/logging/Log.h"
#include "opsu/logging/StubLogger.h"

using namespace opsu;
namespace fs = std::filesystem;
using Status = Download::Status;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static std::string str(const std::optional<std::string>& o) { return o ? *o : std::string("<null>"); }
static uint8_t pat(size_t i) { return static_cast<uint8_t>(i * 31 + 7); }

static bool waitFor(const std::function<bool()>& p, int ms = 5000) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        if (p()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return p();
}

// ---- scripted channel: serves pat(i) for i < size, optional EOF / failure points, optional gating ----
class FakeChannel : public ReadableByteChannel {
public:
    size_t size, eofAt, failAt = static_cast<size_t>(-1);
    bool gated = false;
    size_t allowed = 0;
    size_t maxChunk = 1000;
    explicit FakeChannel(size_t n) : size(n), eofAt(n) {}
    int read(uint8_t* buf, size_t len) override {
        std::unique_lock<std::mutex> lk(mu);
        for (;;) {
            if (closed) throw java::IOException("ClosedChannelException");
            if (pos >= failAt) throw java::IOException("connection reset");
            size_t limit = std::min(eofAt, gated ? allowed : eofAt);
            if (pos >= eofAt) return -1;
            if (pos < limit) {
                size_t n = std::min({len, maxChunk, limit - pos});
                for (size_t i = 0; i < n; i++) buf[i] = pat(pos + i);
                pos += n;
                return static_cast<int>(n);
            }
            cv.wait(lk);
        }
    }
    bool isOpen() const override { std::lock_guard<std::mutex> lk(mu); return !closed; }
    void close() override { std::lock_guard<std::mutex> lk(mu); closed = true; cv.notify_all(); }
    void release(size_t n) { std::lock_guard<std::mutex> lk(mu); allowed += n; cv.notify_all(); }
private:
    mutable std::mutex mu;
    std::condition_variable cv;
    size_t pos = 0;
    bool closed = false;
};

struct Script {
    int code = 200;
    std::map<std::string, std::string> headers;
    int contentLength = -1;
    std::shared_ptr<FakeChannel> channel;
};

class FakeResponse : public HttpResponse {
public:
    FakeResponse(Url u, Script s) : u_(std::move(u)), s_(std::move(s)) {}
    int statusCode() const override { return s_.code; }
    const Url& url() const override { return u_; }
    std::optional<std::string> header(const std::string& n) const override {
        for (auto& kv : s_.headers) if (kv.first == n) return kv.second;
        return std::nullopt;
    }
    int contentLength() const override { return s_.contentLength; }
    ReadableByteChannel& body() override {
        if (s_.code >= 400) throw java::IOException("Server returned HTTP response code: " + std::to_string(s_.code));
        return *s_.channel;
    }
private:
    Url u_;
    Script s_;
};

class FakeClient : public HttpClient {
public:
    std::map<std::string, Script> scripts;  // by url.toString()
    std::vector<HttpRequest> requests;
    std::mutex mu;
    std::unique_ptr<HttpResponse> open(const HttpRequest& r) override {
        std::lock_guard<std::mutex> lk(mu);
        requests.push_back(r);
        auto it = scripts.find(r.url.toString());
        if (it == scripts.end()) throw java::IOException("connect failed: " + r.url.toString());
        return std::make_unique<FakeResponse>(r.url, it->second);
    }
};

static Script ok(size_t n, int declared = -2) {
    Script s;
    s.channel = std::make_shared<FakeChannel>(n);
    s.contentLength = declared == -2 ? static_cast<int>(n) : declared;
    return s;
}
static Script redirect(int code, const std::string& loc) {
    Script s; s.code = code;
    if (!loc.empty()) s.headers["Location"] = loc;
    s.channel = std::make_shared<FakeChannel>(0);
    return s;
}

static std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}
static std::string expected(size_t n) { std::string s(n, 0); for (size_t i = 0; i < n; i++) s[i] = static_cast<char>(pat(i)); return s; }

struct Counters {
    std::atomic<int> completed{0}, errors{0};
    Download::Listener listener() {
        Download::Listener l;
        l.completed = [this] { completed++; };
        l.error = [this] { errors++; };
        return l;
    }
};

// captures ErrorHandler calls
struct Errs {
    std::mutex mu;
    std::vector<std::string> msgs;
    std::vector<bool> reports, hadEx;
    Errs() {
        Download::setErrorHandler([this](const std::string& m, const std::exception* e, bool r) {
            std::lock_guard<std::mutex> lk(mu); msgs.push_back(m); reports.push_back(r); hadEx.push_back(e != nullptr);
        });
    }
    ~Errs() { Download::setErrorHandler(nullptr); }
};

int main() {
    StubLogger& stub = StubLogger::get();
    Log::setLogger(&stub);
    fs::path tmp = fs::temp_directory_path() / ("opsu_b17_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    const std::string T = tmp.u8string();

    // ================= Url =================
    {
        Url u = Url::parse("HTTPS://User@Example.com:8080/a/b.osz?x=1&y=2#frag");
        CHECK(u.getProtocol() == "https" && u.getHost() == "Example.com" && u.getPort() == 8080);
        CHECK(u.getPath() == "/a/b.osz" && u.getQuery() == "x=1&y=2" && u.getRef() == "frag" && u.getAuthority() == "User@Example.com:8080");
        CHECK(u.toString() == "https://User@Example.com:8080/a/b.osz?x=1&y=2#frag");
        Url v = Url::parse("http://host");
        CHECK(v.getPath().empty() && v.getPort() == -1 && v.toString() == "http://host" && !v.hasQuery());
        CHECK(Url::parse("http://h/p?").toString() == "http://h/p?" && Url::parse("http://h/p?").hasQuery());
        CHECK(Url::parse("http://h:abc/").getPort() == -1);
        CHECK(Url::parse("ftp://h/x").getProtocol() == "ftp" && Url::parse("file:///tmp/x").getPath() == "/tmp/x");
        for (const char* bad : {"gopher://h/", "notaurl", "", "://h", "1http://h", "/relative", "ht tp://h"}) {
            bool threw = false;
            try { Url::parse(bad); } catch (const MalformedURLException&) { threw = true; }
            CHECK(threw);
        }
        bool isIo = false; try { Url::parse("x"); } catch (const java::IOException&) { isIo = true; }
        CHECK(isIo);  // MalformedURLException is an IOException (Download relies on it)

        Url base = Url::parse("http://h.com/a/b/c.osz?q=1#r");
        CHECK(Url::resolve(base, "https://o.org/z").toString() == "https://o.org/z");
        CHECK(Url::resolve(base, "//cdn.net/f").toString() == "http://cdn.net/f");
        CHECK(Url::resolve(base, "/root/f.osz").toString() == "http://h.com/root/f.osz");
        CHECK(Url::resolve(base, "d.osz").toString() == "http://h.com/a/b/d.osz");
        CHECK(Url::resolve(base, "../d.osz?z=1").toString() == "http://h.com/a/d.osz?z=1");
        CHECK(Url::resolve(base, "../../../d").toString() == "http://h.com/d");
        CHECK(Url::resolve(base, "./x/./y/../z").toString() == "http://h.com/a/b/x/z");
        CHECK(Url::resolve(base, "?n=2").toString() == "http://h.com/a/b/c.osz?n=2");
        CHECK(Url::resolve(base, "#new").toString() == "http://h.com/a/b/c.osz?q=1#new");
        CHECK(Url::resolve(base, "").toString() == base.toString());
        CHECK(Url::resolve(Url::parse("http://h.com"), "x").toString() == "http://h.com/x");
        CHECK(Url::resolve(base, "dir/").toString() == "http://h.com/a/b/dir/");
        CHECK(Url::resolve(base, "..").toString() == "http://h.com/a/");
    }

    // ================= Download: construction =================
    {
        Errs errs;
        Download d("not a url", T + "/x");
        CHECK(d.getStatus() == Status::ERROR && !d.isActive() && !d.getRemoteURL());
        CHECK(errs.msgs.size() == 1 && errs.msgs[0] == "Bad download URL: 'not a url'" && errs.reports[0] && errs.hadEx[0]);
        CHECK(!d.start());
        CHECK(d.getProgress() == -1.f && d.readSoFar() == 0 && d.contentLength() == -1);
        Download g("gopher://x/y", T + "/x");
        CHECK(g.getStatus() == Status::ERROR);
        Download w("http://h/f", T + "/x");
        CHECK(w.getStatus() == Status::WAITING && w.isActive() && w.getProgress() == 0.f && !w.isTransferring());
        CHECK(str(w.getDownloadSpeed()) == "" && str(w.getTimeRemaining()) == "");   // the getters run the update first: not downloading -> ""
        CHECK(w.getRemoteURL()->toString() == "http://h/f");
        CHECK(std::string(Download::getName(Status::WAITING)) == "Waiting" && std::string(Download::getName(Status::DOWNLOADING)) == "Downloading" &&
              std::string(Download::getName(Status::COMPLETE)) == "Complete" && std::string(Download::getName(Status::CANCELLED)) == "Cancelled" &&
              std::string(Download::getName(Status::ERROR)) == "Error");
    }

    // ================= Download: success paths =================
    {
        FakeClient c;
        c.scripts["http://h/f"] = ok(20000);
        Counters cnt;
        Download d("http://h/f", T + "/set1", std::string("1 A:rt - T?tle.osz"));
        d.setListener(cnt.listener());
        d.setHttpClient(&c);
        CHECK(d.start());
        d.join();
        CHECK(d.getStatus() == Status::COMPLETE && !d.isActive() && !d.isTransferring());
        CHECK(cnt.completed == 1 && cnt.errors == 0);
        CHECK(d.contentLength() == 20000 && d.readSoFar() == 20000 && d.getProgress() == 100.f);
        CHECK(!fs::exists(tmp / "set1"));
        CHECK(readFile(tmp / "1 A-rt - T-tle.osz") == expected(20000));   // renamed + cleaned with '-'
        CHECK(!d.start());                                                // no longer WAITING
        CHECK(c.requests.size() == 1 && c.requests[0].headers.at("User-Agent") == Download::USER_AGENT);
        CHECK(c.requests[0].connectTimeoutMs == 5000 && c.requests[0].readTimeoutMs == 10000 && !c.requests[0].disableSSLCertValidation);
    }
    {   // no rename; zero length; exact multiples of the 8192 block
        FakeClient c;
        c.scripts["http://h/a"] = ok(8192);
        c.scripts["http://h/b"] = ok(0);
        c.scripts["http://h/c"] = ok(8193);
        Download a("http://h/a", T + "/a.bin"); a.setHttpClient(&c); a.start(); a.join();
        Download b("http://h/b", T + "/b.bin"); b.setHttpClient(&c); b.start(); b.join();
        Download e("http://h/c", T + "/c.bin"); e.setHttpClient(&c); e.start(); e.join();
        CHECK(a.getStatus() == Status::COMPLETE && readFile(tmp / "a.bin") == expected(8192));
        CHECK(b.getStatus() == Status::COMPLETE && fs::exists(tmp / "b.bin") && fs::file_size(tmp / "b.bin") == 0 && b.getProgress() == 100.f);
        CHECK(e.getStatus() == Status::COMPLETE && readFile(tmp / "c.bin") == expected(8193));
    }

    // ================= Download: request headers / ssl =================
    {
        FakeClient c;
        c.scripts["http://h/f"] = ok(10);
        Download d("http://h/f", T + "/h.bin");
        d.setHttpClient(&c);
        d.setRequestHeaders({{"user-agent", "Custom/1"}, {"X-Key", "v"}});
        d.setSSLCertValidation(false);
        d.start(); d.join();
        CHECK(d.getStatus() == Status::COMPLETE);
        CHECK(c.requests.at(0).headers.size() == 2 && c.requests[0].headers.at("user-agent") == "Custom/1" && c.requests[0].headers.at("X-Key") == "v");
        CHECK(c.requests[0].headers.count("User-Agent") == 0);               // replaced case-insensitively, like setRequestProperty
        CHECK(c.requests[0].disableSSLCertValidation);
        FakeClient c2; c2.scripts["http://h/f"] = ok(10);
        Download d2("http://h/f", T + "/h2.bin"); d2.setHttpClient(&c2); d2.setSSLCertValidation(false); d2.setSSLCertValidation(true);
        d2.start(); d2.join();
        CHECK(!c2.requests.at(0).disableSSLCertValidation);
    }

    // ================= Download: redirects =================
    {
        Errs errs;
        FakeClient c;
        c.scripts["http://h/0"] = redirect(301, "/1");
        c.scripts["http://h/1"] = redirect(302, "2");
        c.scripts["http://h/2"] = redirect(303, "https://s/3");
        c.scripts["https://s/3"] = redirect(305, "//t/4");
        c.scripts["https://t/4"] = ok(100);
        Counters cnt;
        Download d("http://h/0", T + "/r1.bin"); d.setListener(cnt.listener()); d.setHttpClient(&c);
        d.start(); d.join();
        CHECK(d.getStatus() == Status::COMPLETE && readFile(tmp / "r1.bin") == expected(100));   // 4 redirects are allowed
        CHECK(c.requests.size() == 5 && c.requests[4].url.toString() == "https://t/4" && errs.msgs.empty());
    }
    {   // the 5th redirect is refused ("over 3")
        Errs errs;
        FakeClient c;
        c.scripts["http://h/0"] = redirect(302, "/1"); c.scripts["http://h/1"] = redirect(302, "/2");
        c.scripts["http://h/2"] = redirect(302, "/3"); c.scripts["http://h/3"] = redirect(302, "/4");
        c.scripts["http://h/4"] = redirect(302, "/5"); c.scripts["http://h/5"] = ok(10);
        Counters cnt;
        Download d("http://h/0", T + "/r2.bin"); d.setListener(cnt.listener()); d.setHttpClient(&c);
        d.start(); d.join();
        CHECK(d.getStatus() == Status::ERROR && cnt.errors == 1 && cnt.completed == 0 && c.requests.size() == 5);
        CHECK(errs.msgs.size() == 1 && errs.msgs[0] == "Download for URL 'http://h/4' is attempting too many redirects (over 3)." && !errs.reports[0] && !errs.hadEx[0]);
        CHECK(!fs::exists(tmp / "r2.bin"));
    }
    {   // no Location header; non-http target; malformed target; 307 / 308 are not treated as redirects
        Errs errs;
        FakeClient c;
        c.scripts["http://h/nl"] = redirect(302, "");
        c.scripts["http://h/ftp"] = redirect(302, "ftp://h/f");
        c.scripts["http://h/bad"] = redirect(302, "gopher://h/f");
        c.scripts["http://h/307"] = redirect(307, "/x"); c.scripts["http://h/307"].contentLength = 5; c.scripts["http://h/307"].channel = std::make_shared<FakeChannel>(5);
        Counters cnt;
        auto run = [&](const char* url, const char* file) {
            auto d = std::make_unique<Download>(url, T + "/" + file); d->setListener(cnt.listener()); d->setHttpClient(&c);
            d->start(); d->join(); return d;
        };
        auto a = run("http://h/nl", "n1");
        CHECK(a->getStatus() == Status::ERROR && errs.msgs.at(0) == "Download for URL 'http://h/nl' is attempting to redirect without a 'location' header.");
        auto b = run("http://h/ftp", "n2");
        CHECK(b->getStatus() == Status::ERROR && errs.msgs.at(1) == "Download for URL 'http://h/ftp' is attempting to redirect to a non-HTTP/HTTPS protocol 'ftp'.");
        auto m = run("http://h/bad", "n3");
        CHECK(m->getStatus() == Status::ERROR && errs.msgs.size() == 2);       // MalformedURLException: no error-handler call, just ERROR
        auto s = run("http://h/307", "n4");
        CHECK(s->getStatus() == Status::COMPLETE && readFile(tmp / "n4") == expected(5));
        CHECK(cnt.errors == 3 && cnt.completed == 1);
    }

    // ================= Download: failures =================
    {
        FakeClient c;
        Counters cnt;
        c.scripts["http://h/chunked"] = ok(10, -1);
        c.scripts["http://h/404"] = ok(10); c.scripts["http://h/404"].code = 404;
        c.scripts["http://h/fail"] = ok(30000); c.scripts["http://h/fail"].channel->failAt = 12000;
        c.scripts["http://h/short"] = ok(5000, 9000); c.scripts["http://h/short"].channel->eofAt = 5000;
        auto run = [&](const char* url, const char* file) {
            auto d = std::make_unique<Download>(url, T + "/" + file); d->setListener(cnt.listener()); d->setHttpClient(&c);
            d->start(); d->join(); return d;
        };
        auto a = run("http://h/chunked", "f1");
        CHECK(a->getStatus() == Status::ERROR && cnt.errors == 1 && a->contentLength() == -1 && !fs::exists(tmp / "f1"));
        auto b = run("http://h/404", "f2");
        CHECK(b->getStatus() == Status::ERROR && cnt.errors == 2 && !fs::exists(tmp / "f2"));
        auto d = run("http://h/fail", "f3");
        // Java quirk kept: listener.error() is called, then the status is overwritten with CANCELLED
        CHECK(d->getStatus() == Status::CANCELLED && cnt.errors == 3 && cnt.completed == 0);
        CHECK(d->getProgress() == -1.f && d->readSoFar() == 0 && !d->isTransferring());
        CHECK(!c.scripts["http://h/fail"].channel->isOpen());              // the stream is closed on every exit path
        auto e = run("http://h/short", "f4");   // truncated stream: Java spun forever; now ERROR (the "bytes expected" branch)
        CHECK(e->getStatus() == Status::ERROR && cnt.errors == 4 && cnt.completed == 0 && !c.scripts["http://h/short"].channel->isOpen());
        CHECK(fs::exists(tmp / "f4") && fs::file_size(tmp / "f4") == 5000);
        auto g = run("http://h/missing", "f5");  // connection error
        CHECK(g->getStatus() == Status::ERROR && cnt.errors == 5);
        Download nc("http://h/x", T + "/f6"); Counters c2; nc.setListener(c2.listener());   // no client set
        nc.start(); nc.join();
        CHECK(nc.getStatus() == Status::ERROR && c2.errors == 1);
        Download unw("http://h/x", T + "/nodir/sub/f7"); unw.setHttpClient(&c); c.scripts["http://h/x"] = ok(10); unw.setListener(c2.listener());
        unw.start(); unw.join();                  // cannot create the output file
        CHECK(unw.getStatus() == Status::ERROR && c2.errors == 2);
    }

    // ================= Download: cancel =================
    {
        FakeClient c;
        Script s = ok(100000); s.channel->gated = true;
        auto ch = s.channel;
        c.scripts["http://h/f"] = s;
        Counters cnt;
        Download d("http://h/f", T + "/cancel.bin", std::string("cancel.osz"));
        d.setListener(cnt.listener()); d.setHttpClient(&c);
        CHECK(d.start());
        CHECK(!d.start());                                        // a worker already exists
        CHECK(waitFor([&] { return d.getStatus() == Status::DOWNLOADING; }));
        ch->release(3000);
        CHECK(waitFor([&] { return d.readSoFar() == 3000; }));
        CHECK(d.isActive() && d.isTransferring() && ch->isOpen());
        CHECK(std::abs(d.getProgress() - 3.0f) < 1e-4f);          // 3000 / 100000 * 100
        CHECK(fs::exists(tmp / "cancel.bin"));
        d.cancel();
        d.join();
        CHECK(d.getStatus() == Status::CANCELLED && !d.isActive() && !d.isTransferring());
        CHECK(!fs::exists(tmp / "cancel.bin") && !fs::exists(tmp / "cancel.osz"));   // partial file deleted
        CHECK(cnt.errors == 0 && cnt.completed == 0 && !ch->isOpen());
        CHECK(d.getProgress() == -1.f && d.readSoFar() == 0);
    }
    {   // cancel while waiting: status CANCELLED, never starts
        FakeClient c; c.scripts["http://h/f"] = ok(10);
        Download d("http://h/f", T + "/cw.bin"); d.setHttpClient(&c);
        d.cancel(); d.join();
        CHECK(d.getStatus() == Status::CANCELLED && !d.start() && c.requests.empty());
        d.cancel(); d.cancel(); d.join();                             // repeated cancels are fine
        CHECK(d.getStatus() == Status::CANCELLED);
    }
    {   // destroying an active download stops it (does not hang), and cancel after completion keeps the file
        FakeClient c;
        Script s = ok(100000); s.channel->gated = true;
        c.scripts["http://h/f"] = s;
        {
            Download d("http://h/f", T + "/destroy.bin"); d.setHttpClient(&c); d.start();
            CHECK(waitFor([&] { return d.getStatus() == Status::DOWNLOADING; }));
        }
        c.scripts["http://h/g"] = ok(50);
        Download d2("http://h/g", T + "/done.bin"); d2.setHttpClient(&c); d2.start(); d2.join();
        d2.cancel(); d2.join();
        CHECK(fs::exists(tmp / "done.bin"));                          // not transferring any more: file kept (status CANCELLED, like Java)
    }

    // ================= Download: speed / ETA =================
    {
        FakeClient c;
        Script s = ok(20000000); s.channel->gated = true; s.channel->maxChunk = 8192;
        auto ch = s.channel;
        c.scripts["http://h/f"] = s;
        std::atomic<int64_t> now{5000000};
        Download d("http://h/f", T + "/speed.bin"); d.setHttpClient(&c);
        d.setClock([&] { return now.load(); });
        d.start();
        CHECK(waitFor([&] { return d.getStatus() == Status::DOWNLOADING; }));
        // start() already ran one update: (0 + 1) * 1000 / (now + 1) = 0 -> "0 B/s" and "?"
        CHECK(waitFor([&] { return d.getDownloadSpeed().has_value(); }));
        CHECK(str(d.getDownloadSpeed()) == Utils::bytesToString(0) + "/s" && str(d.getTimeRemaining()) == "?");
        ch->release(4096);
        CHECK(waitFor([&] { return d.readSoFar() == 4096; }));
        now = 5000000 + 2000;
        CHECK(str(d.getDownloadSpeed()) == Utils::bytesToString(2048) + "/s");   // 4096 * 1000 / 2000
        CHECK(str(d.getTimeRemaining()) == "2h42m43s");                          // (20000000 - 4096) / 2048 = 9763 s
        now = 5000000 + 3000;                                                    // exactly 1000 ms after the last update: not yet (strict >)
        CHECK(str(d.getDownloadSpeed()) == Utils::bytesToString(2048) + "/s" && str(d.getTimeRemaining()) == "2h42m43s");
        now = 5000000 + 2500;                                                    // < 1000 ms later: unchanged
        CHECK(str(d.getDownloadSpeed()) == Utils::bytesToString(2048) + "/s" && str(d.getTimeRemaining()) == "2h42m43s");
        ch->release(10240);
        CHECK(waitFor([&] { return d.readSoFar() == 14336; }));
        now = 5000000 + 4000;
        CHECK(str(d.getDownloadSpeed()) == Utils::bytesToString(2816) + "/s");   // 10240 * 1000 / 2000 = 5120; EWMA 0.25 * 5120 + 0.75 * 2048 = 2816
        CHECK(str(d.getTimeRemaining()) == "1h58m17s");                          // 19985664 / 2816 = 7097
        now = 5000000 + 6000;                                                    // no new bytes: speed 0
        CHECK(str(d.getDownloadSpeed()) == Utils::bytesToString(0) + "/s" && str(d.getTimeRemaining()) == "?");
        ch->release(100000);
        CHECK(waitFor([&] { return d.readSoFar() == 114336; }));
        now = 5000000 + 8000;                                                    // avg was reset to 0, so the new speed is taken as is
        CHECK(str(d.getDownloadSpeed()) == Utils::bytesToString(50000) + "/s");  // 100000 * 1000 / 2000
        CHECK(str(d.getTimeRemaining()) == "6m37s");                             // (20000000 - 114336) / 50000 = 397 s
        d.cancel(); d.join();
        CHECK(str(d.getDownloadSpeed()) == "" && str(d.getTimeRemaining()) == "");   // not downloading: cleared
    }
    {   // small totals: "XmYs" format; and the first-call branch when the clock is small (<= 1000)
        FakeClient c;
        Script s = ok(300000); s.channel->gated = true; s.channel->maxChunk = 8192;
        auto ch = s.channel;
        c.scripts["http://h/f"] = s;
        std::atomic<int64_t> now{500};
        Download d("http://h/f", T + "/speed2.bin"); d.setHttpClient(&c);
        d.setClock([&] { return now.load(); });
        d.start();
        CHECK(waitFor([&] { return d.getStatus() == Status::DOWNLOADING; }));
        CHECK(!d.getDownloadSpeed() && !d.getTimeRemaining());                   // clock 500 is not > -1 + 1000: only the baseline is stored
        ch->release(100000);
        CHECK(waitFor([&] { return d.readSoFar() == 100000; }));
        now = 500 + 2000;
        CHECK(str(d.getDownloadSpeed()) == Utils::bytesToString(50000) + "/s" && str(d.getTimeRemaining()) == "0m4s");   // (300000 - 100000) / 50000
        d.cancel(); d.join();
    }

    // ================= DownloadNode =================
    {
        DownloadNode n(42, "2024-01-02", "Title", "", "Artist", "Art\xE3\x81\x82", "Mapper");
        CHECK(n.getID() == 42 && n.getDate() == "2024-01-02" && n.getCreator() == "Mapper" && n.getHash().empty());
        CHECK(n.getTitle(false) == "Title" && n.getTitle(true) == "Title");                 // empty unicode title falls back
        CHECK(n.getArtist(false) == "Artist" && n.getArtist(true) == "Art\xE3\x81\x82");
        CHECK(n.toString(false) == "[42] Artist - Title (by Mapper)" && n.toString(true) == "[42] Art\xE3\x81\x82 - Title (by Mapper)");
        DownloadNode h(1, "d", "t", "tu", "a", "au", "c", "abcdef");
        CHECK(h.getHash() == "abcdef" && h.getTitle(true) == "tu" && h.getTitle(false) == "t");
        CHECK(n.getDownload() == nullptr);
        // stars: null vs empty, label is %.2f HALF_UP (5.125 -> 5.13; printf would give 5.12)
        CHECK(!n.hasStars() && !n.getStars() && n.getMinStars() == -1 && n.getMaxStars() == -1 && n.getStarLabel().empty());
        n.setStars(std::vector<double>{});
        CHECK(n.getStars().has_value() && !n.hasStars() && n.getMinStars() == -1 && n.getStarLabel().empty());
        n.setStars(std::vector<double>{1.5, 3.25, 5.125});
        CHECK(n.hasStars() && n.getMinStars() == 1.5 && n.getMaxStars() == 5.125 && n.getStarLabel() == "5.13");
        n.setStars(std::nullopt);
        CHECK(!n.getStars() && n.getStarLabel().empty());
        // set info / difficulties / status
        CHECK(n.getBpm() == -1 && n.getSource().empty() && n.getFavourites() == -1 && n.getPlays() == -1 && !n.hasVideo() && !n.hasStoryboard());
        n.setSetInfo(180.5, "Src", 7, 99, true, false);
        CHECK(n.getBpm() == 180.5 && n.getSource() == "Src" && n.getFavourites() == 7 && n.getPlays() == 99 && n.hasVideo() && !n.hasStoryboard());
        CHECK(!n.getDifficulties());
        DownloadNode::Difficulty df; CHECK(df.version.empty() && df.stars == -1 && df.length == -1 && df.circles == -1 && df.spinners == -1);
        df.version = "Insane";
        n.setDifficulties(std::vector<DownloadNode::Difficulty>{df});
        CHECK(n.getDifficulties() && n.getDifficulties()->size() == 1 && (*n.getDifficulties())[0].version == "Insane");
        CHECK(!n.getStatusLabel() && n.getStatusColor() == StatusColor::GRAY);
        struct { int st; const char* label; StatusColor col; } rows[] = {
            {1, "Ranked", StatusColor::GREEN}, {2, "Approved", StatusColor::GREEN}, {3, "Qualified", StatusColor::CYAN},
            {4, "Loved", StatusColor::MAGENTA}, {0, "Pending", StatusColor::ORANGE}, {-1, "WIP", StatusColor::ORANGE},
            {-2, "Graveyard", StatusColor::GRAY}};
        for (auto& r : rows) { n.setStatus(r.st); CHECK(str(n.getStatusLabel()) == r.label && n.getStatusColor() == r.col); }
        n.setStatus(5);  CHECK(!n.getStatusLabel() && n.getStatusColor() == StatusColor::GRAY);
        n.setStatus(-99); CHECK(!n.getStatusLabel());
        // ease / smoothing
        CHECK(DownloadNode::ease(0.f, 1.f, 0.5f) == 0.5f && DownloadNode::ease(0.999f, 1.f, 0.5f) == 1.f && DownloadNode::ease(1.001f, 1.f, 0.1f) == 1.f);
        CHECK(DownloadNode::ease(1.f, 0.f, 0.25f) == 0.75f && DownloadNode::ease(0.002f, 0.f, 0.5f) == 0.001f);
        CHECK(DownloadNode::smoothing(0) == 0.f && DownloadNode::smoothing(70) == 0.5f && DownloadNode::smoothing(210) == 0.75f);
        CHECK(DownloadNode::COLUMNS == 3);
    }
    {   // createDownload
        struct Src : DownloadSource {
            std::map<int, std::string> urls;
            std::map<std::string, std::string> hdr{{"X-A", "1"}};
            bool noSSL = true;
            std::optional<std::string> getDownloadURL(int id) override { auto it = urls.find(id); return it == urls.end() ? std::nullopt : std::optional<std::string>(it->second); }
            std::map<std::string, std::string> getDownloadRequestHeaders() override { return hdr; }
            bool disableSSLInDownloads() override { return noSSL; }
        } src;
        src.urls[7] = "http://h/dl/7";
        FakeClient c; c.scripts["http://h/dl/7"] = ok(1234);
        std::vector<std::pair<std::string, NotifyKind>> notes;
        auto notify = [&](const std::string& m, NotifyKind k) { notes.emplace_back(m, k); };
        DownloadNode none(8, "d", "T", "", "A", "", "C");
        none.createDownload(src, T, false, &c, notify);
        CHECK(none.getDownload() == nullptr);                                            // server has no URL: nothing created
        DownloadNode n(7, "d", "My:Title", "\xE3\x82\xBF", "Art", "\xE3\x82\xA2", "C");
        n.createDownload(src, T, true, &c, notify);
        Download* d = n.getDownload();
        CHECK(d != nullptr && d->getStatus() == Status::WAITING && d->getRemoteURL()->toString() == "http://h/dl/7");
        src.urls[7] = "http://h/other";
        n.createDownload(src, T, true, &c, notify);                                      // already has one: no-op
        CHECK(n.getDownload() == d);
        d->start(); d->join();
        CHECK(d->getStatus() == Status::COMPLETE);
        // saved as <importDir>/7, then renamed to "<id> <artist> - <title>.osz" from the RAW fields, illegal chars replaced by '-'
        CHECK(readFile(tmp / "7 Art - My-Title.osz") == expected(1234) && !fs::exists(tmp / "7"));
        CHECK(notes.size() == 1 && notes[0].first == "Download complete: \xE3\x82\xBF" && notes[0].second == NotifyKind::SUCCESS);
        CHECK(c.requests.at(0).headers.at("X-A") == "1" && c.requests[0].disableSSLCertValidation);
        n.clearDownload();
        CHECK(n.getDownload() == nullptr);
        {   // while it runs the data is written to <importDir>/<id>
            src.urls[12] = "http://h/dl/12";
            Script gs = ok(5000); gs.channel->gated = true; auto gch = gs.channel;
            c.scripts["http://h/dl/12"] = gs;
            DownloadNode g(12, "d", "G", "", "A", "", "C");
            g.createDownload(src, T, false, &c, nullptr);
            g.getDownload()->start();
            CHECK(waitFor([&] { return g.getDownload()->getStatus() == Status::DOWNLOADING; }));
            CHECK(fs::exists(tmp / "12"));
            gch->release(5000);
            g.getDownload()->join();
            CHECK(g.getDownload()->getStatus() == Status::COMPLETE && !fs::exists(tmp / "12") && fs::exists(tmp / "12 A - G.osz"));
        }
        // error notification
        src.urls[9] = "http://h/missing";
        DownloadNode e(9, "d", "T", "", "A", "", "C");
        e.createDownload(src, T, false, &c, notify);
        e.getDownload()->start(); e.getDownload()->join();
        CHECK(e.getDownload()->getStatus() == Status::ERROR && notes.size() == 2 &&
              notes[1].first == "Download failed due to a server error. Please select another beatmap" && notes[1].second == NotifyKind::FAILURE);
        e.createDownload(src, T, false, &c, nullptr);                                    // still has its (finished) download: no-op
        DownloadNode q(10, "d", "T", "", "A", "", "C");
        src.urls[10] = "http://h/missing"; q.createDownload(src, T, false, &c, nullptr);   // null notifier is fine
        q.getDownload()->start(); q.getDownload()->join();
        CHECK(q.getDownload()->getStatus() == Status::ERROR);
        src.noSSL = false; src.urls[11] = "http://h/dl/11"; c.scripts["http://h/dl/11"] = ok(3);
        DownloadNode s(11, "d", "T", "", "A", "", "C");
        s.createDownload(src, T, false, &c, nullptr);
        s.getDownload()->start(); s.getDownload()->join();
        CHECK(!c.requests.back().disableSSLCertValidation);
    }

    // ================= DownloadList =================
    {
        CHECK(DownloadList::exitConfirmation() == "Beatmap downloads are in progress.\nAre you sure you want to quit opsu!?");
        CHECK(&DownloadList::get() == &DownloadList::get());

        struct Src : DownloadSource {
            std::map<int, std::string> urls;
            std::optional<std::string> getDownloadURL(int id) override { auto it = urls.find(id); return it == urls.end() ? std::nullopt : std::optional<std::string>(it->second); }
            std::map<std::string, std::string> getDownloadRequestHeaders() override { return {}; }
            bool disableSSLInDownloads() override { return false; }
        } src;
        FakeClient c;
        c.scripts["http://h/done"] = ok(10);
        auto mk = [&](int id, const std::string& url) {
            auto n = std::make_shared<DownloadNode>(id, "d", "T" + std::to_string(id), "", "A", "", "C");
            if (!url.empty()) { src.urls[id] = url; n->createDownload(src, T, false, &c, nullptr); }
            return n;
        };
        DownloadList l;
        CHECK(l.isEmpty() && l.size() == 0 && !l.hasActiveDownloads() && l.getNode(0) == nullptr && l.getNode(-1) == nullptr);
        CHECK(!l.contains(1) && l.getDownload(1) == nullptr);
        l.remove(0); l.remove(-1); l.remove(5); l.cancelAllDownloads(); l.clearInactiveDownloads(); l.clearDownloads(Status::ERROR);   // no-ops

        auto nodeNoDl = mk(1, "");                       // node without a Download
        auto waiting = mk(2, "http://h/waiting");        // WAITING (active)
        auto bad = mk(3, "no-protocol");                 // ERROR (not active)
        auto done = mk(4, "http://h/done");              // COMPLETE
        done->getDownload()->start(); done->getDownload()->join();
        CHECK(done->getDownload()->getStatus() == Status::COMPLETE && bad->getDownload()->getStatus() == Status::ERROR);
        for (auto& n : {nodeNoDl, waiting, bad, done}) l.addNode(n);
        CHECK(l.size() == 4 && !l.isEmpty() && l.contains(1) && l.contains(4) && !l.contains(5));
        CHECK(l.getNode(0) == nodeNoDl && l.getNode(3) == done && l.getNode(4) == nullptr && l.getNode(-1) == nullptr);
        CHECK(l.getDownload(1) == nullptr && l.getDownload(2) == waiting->getDownload() && l.getDownload(3) == bad->getDownload() && l.getDownload(99) == nullptr);
        CHECK(l.hasActiveDownloads());

        l.clearDownloads(Status::CANCELLED);             // matches nothing
        CHECK(l.size() == 4);
        l.clearDownloads(Status::ERROR);                 // removes `bad`, clears its Download
        CHECK(l.size() == 3 && !l.contains(3) && bad->getDownload() == nullptr && l.getNode(2) == done);
        l.clearInactiveDownloads();                      // removes `done` (COMPLETE); keeps no-download node and the waiting one
        CHECK(l.size() == 2 && !l.contains(4) && done->getDownload() == nullptr && l.contains(1) && l.contains(2));
        CHECK(l.getNode(0) == nodeNoDl && l.getNode(1) == waiting);

        l.cancelAllDownloads();                          // only active ones are cancelled
        waiting->getDownload()->join();
        CHECK(waiting->getDownload()->getStatus() == Status::CANCELLED && !l.hasActiveDownloads());
        l.clearDownloads(Status::CANCELLED);
        CHECK(l.size() == 1 && l.getNode(0) == nodeNoDl && waiting->getDownload() == nullptr);

        // remove by node / by index
        auto n5 = mk(5, ""), n6 = mk(6, "");
        l.addNode(n5); l.addNode(n6);
        l.remove(n5.get());
        CHECK(l.size() == 2 && !l.contains(5) && l.contains(6) && l.getNode(1) == n6);
        DownloadNode stranger(77, "d", "t", "", "a", "", "c");
        l.remove(&stranger); l.remove(nullptr);          // not in the list: nothing
        CHECK(l.size() == 2);
        l.remove(1);
        CHECK(l.size() == 1 && !l.contains(6));
        l.remove(1); l.remove(-1);
        CHECK(l.size() == 1);

        // Java quirk: duplicate IDs share one map entry
        auto a1 = mk(8, ""), a2 = mk(8, "http://h/waiting2");
        l.addNode(a1); l.addNode(a2);
        CHECK(l.size() == 3 && l.getDownload(8) == a2->getDownload() && a2->getDownload() != nullptr);   // the map points at the later node
        l.remove(a1.get());                              // removes the id from the map although a2 (same id) is still listed
        CHECK(l.size() == 2 && !l.contains(8) && l.getDownload(8) == nullptr && l.getNode(1) == a2);
        a2->clearDownload();

        {   // cancelAllDownloads only touches active downloads
            DownloadList l3;
            c.scripts["http://h/done2"] = ok(10);
            auto cd = mk(40, "http://h/done2"), ce = mk(41, "bad-url"), cw = mk(42, "http://h/waiting");
            cd->getDownload()->start(); cd->getDownload()->join();
            l3.addNode(cd); l3.addNode(ce); l3.addNode(cw);
            l3.cancelAllDownloads();
            cw->getDownload()->join(); cd->getDownload()->join(); ce->getDownload()->join();
            CHECK(cd->getDownload()->getStatus() == Status::COMPLETE && ce->getDownload()->getStatus() == Status::ERROR &&
                  cw->getDownload()->getStatus() == Status::CANCELLED);
        }

        // clearInactive keeps going after removals (consecutive removable nodes)
        DownloadList l2;
        c.scripts["http://h/e"] = ok(1);
        std::vector<std::shared_ptr<DownloadNode>> bads;
        for (int i = 20; i < 26; i++) { bads.push_back(mk(i, "bad-url")); l2.addNode(bads.back()); }
        auto keep = mk(30, "http://h/waiting"); l2.addNode(keep);
        l2.clearInactiveDownloads();
        CHECK(l2.size() == 1 && l2.getNode(0) == keep && l2.contains(30) && !l2.contains(20) && !l2.contains(25));
        CHECK(l2.hasActiveDownloads());
        l2.cancelAllDownloads(); keep->getDownload()->join();
    }

    fs::remove_all(tmp);
    Log::setLogger(nullptr);
    if (failures == 0) std::printf("batch17 OK\n");
    return failures == 0 ? 0 : 1;
}
