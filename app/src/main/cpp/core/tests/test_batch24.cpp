// Tests for batch 24: DownloadServer::reportError / Download::reportError (the ErrorHandler hook). Behavioural.
#include <cstdio>
#include <stdexcept>
#include <string>
#include "opsu/downloads/Download.h"
#include "opsu/downloads/DownloadServer.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

/** Minimal server: the live ones (OsuDirect, Catboy, Nekoha, Hinamizawa) call reportError from their resultList catch block. */
struct ErrServer : DownloadServer {
    using DownloadServer::DownloadServer;
    using DownloadServer::resultList;
    std::string getName() const override { return "Err"; }
    std::optional<std::string> getDownloadURL(int) override { return std::nullopt; }
    std::optional<NodeList> resultList(const std::string&, int, bool) override { return NodeList(); }
    int minQueryLength() override { return 0; }
    int totalResults() override { return -1; }
    static void report(const std::string& m, const std::exception* e, bool r) { reportError(m, e, r); }
};

int main() {
    // reportError forwards to the ErrorHandler hook (message, exception, report flag)
    {
        std::string seen; bool seenReport = false; const std::exception* seenE = nullptr; int calls = 0;
        Download::setErrorHandler([&](const std::string& m, const std::exception* e, bool r) { seen = m; seenE = e; seenReport = r; calls++; });
        std::runtime_error err("x");
        ErrServer::report("boom", &err, true);
        CHECK(calls == 1 && seen == "boom" && seenE == &err && seenReport);
        ErrServer::report("quiet", nullptr, false);
        CHECK(calls == 2 && seen == "quiet" && seenE == nullptr && !seenReport);
        Download::reportError("direct", nullptr, true);                  // the public static on Download itself
        CHECK(calls == 3 && seen == "direct");
        Download::setErrorHandler({});
        Download::reportError("default handler goes to Log::error", nullptr, false);   // must not crash
        CHECK(calls == 3);
    }

    if (failures) { std::printf("batch24: %d FAILED\n", failures); return 1; }
    std::printf("batch24: all tests passed\n");
    return 0;
}
