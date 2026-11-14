// Tests for batch 28 (NekohaServer). Behavioural: a fake HttpClient.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string>
#include "opsu/downloads/servers/NekohaServer.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// ---- fake HTTP ----
struct MemChannel : ReadableByteChannel {
    std::string data; size_t pos = 0; bool open_ = true;
    explicit MemChannel(std::string d) : data(std::move(d)) {}
    int read(uint8_t* buf, std::size_t len) override {
        if (pos >= data.size()) return -1;
        size_t n = std::min<size_t>({len, data.size() - pos, 7});
        std::memcpy(buf, data.data() + pos, n); pos += n; return static_cast<int>(n);
    }
    bool isOpen() const override { return open_; }
    void close() override { open_ = false; }
};
struct FakeResponse : HttpResponse {
    int code; Url u; MemChannel ch;
    FakeResponse(int c, std::string b, Url url) : code(c), u(std::move(url)), ch(std::move(b)) {}
    int statusCode() const override { return code; }
    const Url& url() const override { return u; }
    std::optional<std::string> header(const std::string&) const override { return std::nullopt; }
    int contentLength() const override { return -1; }
    ReadableByteChannel& body() override { return ch; }
};
struct FakeClient : HttpClient {
    struct Reply { int code; std::string body; bool throwIO = false; };
    std::deque<Reply> replies;
    std::vector<HttpRequest> requests;
    std::unique_ptr<HttpResponse> open(const HttpRequest& r) override {
        requests.push_back(r);
        Reply rep = replies.front(); replies.pop_front();
        if (rep.throwIO) throw java::IOException("timeout");
        return std::make_unique<FakeResponse>(rep.code, rep.body, r.url);
    }
};

static std::string item(int id, const std::string& extra = "", const std::string& date = "2023-05-14T23:38:47Z") {
    return R"({"id":)" + std::to_string(id) + R"(,"title":"Song )" + std::to_string(id) + R"(","artist":"Art","creator":"Map",)" +
           R"("last_updated":")" + date + "\"" + (extra.empty() ? "" : "," + extra) + "}";
}
static const std::string BASE = "https://mirror.nekoha.moe/api/search?mode=osu&q=";
static std::string sets(int n, int firstId = 100) {  // {"beatmapsets":[...]}
    std::string s = R"({"beatmapsets":[)";
    for (int i = 0; i < n; i++) s += (i ? "," : "") + item(firstId + i);
    return s + "]}";
}
static std::string wrap(const std::string& arrayBody) { return R"({"beatmapsets":)" + arrayBody + "}"; }

int main() {
    setenv("TZ", "UTC", 1);
    tzset();
    FakeClient fc;
    NekohaServer s(&fc);

    CHECK(s.getName() == "Nekoha" && s.toString() == "Nekoha");
    CHECK(s.getDownloadURL(123) == std::optional<std::string>("https://mirror.nekoha.moe/api/download/123"));
    CHECK(s.getDownloadURL(-5) == std::optional<std::string>("https://mirror.nekoha.moe/api/download/-5"));
    CHECK(s.minQueryLength() == 0 && s.totalResults() == -1 && !s.disableSSLInDownloads() && !s.isDownloadInBrowser());
    CHECK(s.getDownloadRequestHeaders().empty() && NekohaServer::PAGE_LIMIT == 20 && NekohaServer::API_PAGE_SIZE == 100);

    // ---- request URLs (each call uses a fresh server: the cache would hide repeated URLs) ----
    {
        struct Case { std::string q; int page; int status; std::string url; };
        std::vector<Case> cases = {
            {"foo bar&\xC3\xA9", 3, DownloadServer::STATUS_ALL, BASE + "foo+bar%26%C3%A9&page=1"},
            {"abc", 1, DownloadServer::STATUS_RANKED, BASE + "abc&page=1&status=ranked,approved"},
            {"abc", 1, DownloadServer::STATUS_LOVED, BASE + "abc&page=1&status=loved"},
            {"abc", 1, 0, BASE + "abc&page=1"},
            {"abc", 1, -1, BASE + "abc&page=1"},
            {"", 1, DownloadServer::STATUS_ALL, BASE + "&page=1&sort=updated&order=desc"},
            {"", 1, DownloadServer::STATUS_RANKED, BASE + "&page=1&status=ranked,approved&sort=ranked&order=desc"},
            {"", 1, DownloadServer::STATUS_LOVED, BASE + "&page=1&status=loved&sort=ranked&order=desc"},
            {"", 1, 0, BASE + "&page=1&sort=ranked&order=desc"},
            {"x", 5, DownloadServer::STATUS_ALL, BASE + "x&page=1"},     // offset 80 -> still server page 1
            {"x", 6, DownloadServer::STATUS_ALL, BASE + "x&page=2"},     // offset 100 -> server page 2
            {"x", 11, DownloadServer::STATUS_ALL, BASE + "x&page=3"},    // offset 200
        };
        for (const Case& c : cases) {
            FakeClient f2; NekohaServer s2(&f2);
            f2.replies = {{200, R"({"beatmapsets":[]})"}};
            s2.resultList(c.q, c.page, c.status);
            CHECK(f2.requests.size() == 1 && f2.requests[0].url.toString() == c.url);
            if (f2.requests.size() == 1) {
                CHECK(f2.requests[0].connectTimeoutMs == 10000 && f2.requests[0].readTimeoutMs == 15000);
                CHECK(f2.requests[0].headers.at("Accept") == "application/json" && f2.requests[0].headers.at("User-Agent") == "opsu!");
            }
        }
        FakeClient f3; NekohaServer s3(&f3);                           // bool overload
        f3.replies = {{200, R"({"beatmapsets":[]})"}, {200, R"({"beatmapsets":[]})"}};
        s3.resultList("abc", 1, true); s3.resultList("abc", 1, false);
        CHECK(f3.requests.size() == 2 && f3.requests[0].url.toString() == BASE + "abc&page=1&status=ranked,approved" && f3.requests[1].url.toString() == BASE + "abc&page=1");
    }
    // ---- slicing: 100 sets, UI pages of 20, ids 100..199 ----
    {
        FakeClient f; NekohaServer n(&f);
        f.replies = {{200, sets(100)}};
        auto r = n.resultList("q", 1, false);
        CHECK(r && r->size() == 20 && (*r)[0]->getID() == 100 && (*r)[19]->getID() == 119);
        CHECK(n.totalResults() == 0 + 20 + 1);                               // slice ends before the array end
        r = n.resultList("q", 3, false);                                    // cached: no new reply queued
        CHECK(r && r->size() == 20 && (*r)[0]->getID() == 140 && f.requests.size() == 1 && n.totalResults() == 40 + 20 + 1);
        r = n.resultList("q", 5, false);                                    // last slice of a FULL server page: still "more"
        CHECK(r && r->size() == 20 && (*r)[19]->getID() == 199 && f.requests.size() == 1 && n.totalResults() == 80 + 20 + 1);
        // a different status or query is a different key -> network again, and the cache is replaced
        f.replies = {{200, sets(100, 500)}};
        r = n.resultList("q", 1, DownloadServer::STATUS_LOVED);
        CHECK(f.requests.size() == 2 && r && (*r)[0]->getID() == 500);
        f.replies = {{200, sets(100, 700)}};
        r = n.resultList("q", 1, false);                                    // back to the first key: it was replaced, so network again
        CHECK(f.requests.size() == 3 && r && (*r)[0]->getID() == 700);
        // the next server page
        f.replies = {{200, sets(30, 900)}};
        r = n.resultList("q", 6, false);
        CHECK(f.requests.size() == 4 && f.requests[3].url.toString() == BASE + "q&page=2" && r && r->size() == 20 && (*r)[0]->getID() == 900);
        CHECK(n.totalResults() == 100 + 20 + 1);                             // end 20 < 30
        r = n.resultList("q", 7, false);                                    // offset 120 -> start 20 of server page 2: 10 left
        CHECK(f.requests.size() == 4 && r && r->size() == 10 && (*r)[0]->getID() == 920 && n.totalResults() == 120 + 10 + 0);
        r = n.resultList("q", 8, false);                                    // start 40 > 30 sets: empty, no more
        CHECK(f.requests.size() == 4 && r && r->empty() && n.totalResults() == 140);
    }
    // ---- estimates for a short server page ----
    {
        FakeClient f; NekohaServer n(&f);
        f.replies = {{200, sets(10)}};
        auto r = n.resultList("q", 1, false);
        CHECK(r && r->size() == 10 && n.totalResults() == 10);
        f.replies = {{200, sets(20)}};
        n.resultList("q2", 1, false); CHECK(n.totalResults() == 20);       // exactly 20, array < 100: no more
    }
    // ---- parsing: every field, unicode rule, date; odd entries skipped, estimate counts KEPT nodes ----
    {
        std::string extra = R"("title_unicode":"\u66F2","artist_unicode":"Art","ranked":4,"bpm":175.5,"source":"Src","favourite_count":9,"play_count":1234,)"
                            R"("video":true,"storyboard":false,"beatmaps":[{"difficulty_rating":5.5,"version":"Hard","cs":4},{"difficulty_rating":2.25,"version":"Easy","cs":3}])";
        std::string noTitle = R"({"id":7,"artist":"A","creator":"C"})";
        std::string badId = R"({"id":"x","title":"T","artist":"A","creator":"C"})";
        FakeClient f; NekohaServer n(&f);
        f.replies = {{200, wrap("[" + item(11, extra) + "," + noTitle + "," + item(22, R"("title_unicode":"")", "bad date") + "," + badId + "]")}};
        auto r = n.resultList("q", 1, false);
        CHECK(r && r->size() == 2);
        if (r && r->size() == 2) {
            const DownloadNode& a = *(*r)[0];
            CHECK(a.getID() == 11 && a.getDate() == "14 May 2023 23:38:47" && a.getCreator() == "Map");
            CHECK(a.getTitle(true) == "\xE6\x9B\xB2" && a.getArtist(true) == "Art" && a.getTitle(false) == "Song 11");
            CHECK(a.getBpm() == 175.5 && a.getSource() == "Src" && a.getFavourites() == 9 && a.getPlays() == 1234 && a.hasVideo() && !a.hasStoryboard());
            CHECK(a.getStars() && a.getStars()->size() == 2 && (*a.getStars())[0] == 2.25 && (*a.getStars())[1] == 5.5);
            CHECK((*r)[1]->getID() == 22 && (*r)[1]->getDate() == "bad date" && (*r)[1]->getTitle(true) == "Song 22");
        }
        CHECK(n.totalResults() == 0 + 2 + 0);                                // kept nodes (2), not the 4 raw entries
    }
    // ---- null paths reset the total to -1 and do NOT replace the cache ----
    {
        FakeClient f; NekohaServer n(&f);
        f.replies = {{200, sets(30)}};
        n.resultList("q", 1, false);
        CHECK(n.totalResults() == 21);
        for (auto rep : std::vector<FakeClient::Reply>{{404, ""}, {200, "not json"}, {200, "[1]"}, {200, R"({"a":1})"}, {200, R"({"beatmapsets":5})"}, {200, R"({"beatmapsets":null})"}}) {
            f.replies = {rep};
            CHECK(!n.resultList("other", 1, false).has_value() && n.totalResults() == -1);
        }
        size_t before = f.requests.size();
        auto r = n.resultList("q", 2, false);                                // the old cache is still there
        CHECK(f.requests.size() == before && r && r->size() == 10 && n.totalResults() == 20 + 10);
    }
    // ---- Java quirks: a null item -> uncaught NullPointerException; the cache was already stored ----
    {
        FakeClient f; NekohaServer n(&f);
        f.replies = {{200, sets(5)}};
        n.resultList("q", 1, false);                                         // total 5
        for (std::string body : {wrap("[" + item(1) + ",5]"), wrap("[" + item(1) + ",null]"), wrap("[" + item(1) + ",[1]]")}) {
            FakeClient f2; NekohaServer n2(&f2);
            f2.replies = {{200, body}};
            bool threw = false;
            try { n2.resultList("q", 1, false); } catch (const std::logic_error&) { threw = true; }
            CHECK(threw && n2.totalResults() == -1);
        }
        // a bad element OUTSIDE the slice is never looked at
        std::string outside = "[";
        for (int i = 0; i < 20; i++) outside += item(i) + ",";
        FakeClient f3; NekohaServer n3(&f3);
        f3.replies = {{200, wrap(outside + "5]")}};
        auto r = n3.resultList("q", 1, false);
        CHECK(r && r->size() == 20 && n3.totalResults() == 21);
        // page 0: start -20 -> optJSONObject(-20) is null -> NPE; the server page is cached anyway, page 1 then needs no network
        FakeClient f4; NekohaServer n4(&f4);
        f4.replies = {{200, sets(30)}};
        bool threw = false;
        try { n4.resultList("q", 0, false); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw && n4.totalResults() == -1 && f4.requests.size() == 1 && f4.requests[0].url.toString() == BASE + "q&page=1");
        r = n4.resultList("q", 1, false);
        CHECK(r && r->size() == 20 && f4.requests.size() == 1);
        // page -1 (offset -40) likewise (offset / 100 = 0 -> apiPage 1, start -40)
        threw = false;
        try { n4.resultList("q", -1, false); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw && f4.requests.size() == 1);
    }
    // ---- IOException / no client propagate ----
    {
        FakeClient f; NekohaServer n(&f);
        f.replies = {{200, sets(3)}}; n.resultList("q", 1, false);
        f.replies = {{200, "[]", true}};
        bool threw = false;
        try { n.resultList("z", 1, false); } catch (const java::IOException&) { threw = true; }
        CHECK(threw && n.totalResults() == 3);
        NekohaServer none;
        threw = false;
        try { none.resultList("q", 1, false); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw);
    }
    // ---- lookupSet: the set endpoint first, then a search ----
    {
        FakeClient f; NekohaServer n(&f);
        f.replies = {{200, item(42, R"("ranked":1)")}};
        auto node = n.lookupSet(42, std::string("ignored"));
        CHECK(node && node->getID() == 42 && f.requests.size() == 1);
        CHECK(f.requests[0].url.toString() == "https://mirror.nekoha.moe/api/beatmapset/42");
        f.requests.clear();
        f.replies = {{200, item(43)}, {200, wrap("[" + item(1) + "," + item(42) + "]")}};
        node = n.lookupSet(42, std::string(" some text "));
        CHECK(node && node->getID() == 42 && f.requests.size() == 2);
        CHECK(f.requests[1].url.toString() == BASE + "some+text&page=1");
        f.replies = {{404, ""}};
        CHECK(n.lookupSet(42, std::nullopt) == nullptr);
    }
    {
        NekohaServer n; DownloadSource& src = n;
        CHECK(src.getDownloadURL(9) == std::optional<std::string>("https://mirror.nekoha.moe/api/download/9"));
    }

    if (failures) { std::printf("batch28: %d FAILED\n", failures); return 1; }
    std::printf("batch28: all tests passed\n");
    return 0;
}
