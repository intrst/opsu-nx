// Tests for batch 27 (HinamizawaServer). Behavioural: a fake HttpClient.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string>
#include "opsu/downloads/servers/HinamizawaServer.h"

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
static std::string arrayOf(int n) {
    std::string s = "[";
    for (int i = 0; i < n; i++) s += (i ? "," : "") + item(100 + i);
    return s + "]";
}
static const std::string BASE = "https://mirror.hinamizawa.ai/api/v2/search?mode=0&q=";

int main() {
    setenv("TZ", "UTC", 1);
    tzset();
    FakeClient fc;
    HinamizawaServer s(&fc);

    CHECK(s.getName() == "Hinamizawa" && s.toString() == "Hinamizawa");
    CHECK(s.getDownloadURL(123) == std::optional<std::string>("https://mirror.hinamizawa.ai/api/v1/hinai/d/123"));
    CHECK(s.getDownloadURL(-5) == std::optional<std::string>("https://mirror.hinamizawa.ai/api/v1/hinai/d/-5"));
    CHECK(s.minQueryLength() == 0 && s.totalResults() == -1 && !s.disableSSLInDownloads() && !s.isDownloadInBrowser());
    CHECK(s.getDownloadRequestHeaders().empty() && HinamizawaServer::PAGE_LIMIT == 20);

    // ---- request URLs: sort only for an EMPTY query, unencoded ----
    {
        fc.replies.clear();
        for (int i = 0; i < 8; i++) fc.replies.push_back({200, "[]"});
        fc.requests.clear();
        s.resultList("foo bar&\xC3\xA9", 3, false);
        s.resultList("abc", 1, true);
        s.resultList("", 1, DownloadServer::STATUS_ALL);
        s.resultList("", 2, DownloadServer::STATUS_RANKED);
        s.resultList("abc", 1, DownloadServer::STATUS_LOVED);
        s.resultList("", 1, 0);
        s.resultList("abc", 1, -1);
        s.resultList("abc", 0, false);
        CHECK(fc.requests.size() == 8);
        CHECK(fc.requests[0].url.toString() == BASE + "foo+bar%26%C3%A9&amount=20&offset=40");
        CHECK(fc.requests[1].url.toString() == BASE + "abc&amount=20&offset=0&status=1");
        CHECK(fc.requests[2].url.toString() == BASE + "&amount=20&offset=0&sort=updated:desc");
        CHECK(fc.requests[3].url.toString() == BASE + "&amount=20&offset=20&status=1&sort=ranked_date:desc");
        CHECK(fc.requests[4].url.toString() == BASE + "abc&amount=20&offset=0&status=4");
        CHECK(fc.requests[5].url.toString() == BASE + "&amount=20&offset=0&status=0&sort=ranked_date:desc");
        CHECK(fc.requests[6].url.toString() == BASE + "abc&amount=20&offset=0&status=-1");
        CHECK(fc.requests[7].url.toString() == BASE + "abc&amount=20&offset=-20");
        CHECK(s.totalResults() == -20);
        CHECK(fc.requests[0].connectTimeoutMs == 10000 && fc.requests[0].readTimeoutMs == 15000);
        CHECK(fc.requests[0].headers.at("Accept") == "application/json" && fc.requests[0].headers.at("User-Agent") == "opsu!");
    }
    // ---- parsing: every field, unicode rules, date ----
    {
        std::string extra = R"("title_unicode":"\u66F2","artist_unicode":"Art","ranked":4,"bpm":175.5,"source":"Src","favourite_count":9,"play_count":1234,)"
                            R"("video":true,"storyboard":false,"beatmaps":[{"difficulty_rating":5.5,"version":"Hard","cs":4},{"difficulty_rating":2.25,"version":"Easy","cs":3}])";
        fc.replies = {{200, "[" + item(11, extra) + "," + item(22, R"("title_unicode":"")", "bad date") + "]"}};
        auto r = s.resultList("query", 2, false);
        CHECK(r && r->size() == 2 && (*r)[0] && (*r)[1]);
        const DownloadNode& a = *(*r)[0];
        CHECK(a.getID() == 11 && a.getDate() == "14 May 2023 23:38:47" && a.getCreator() == "Map");
        CHECK(a.getTitle(true) == "\xE6\x9B\xB2" && a.getArtist(true) == "Art" && a.getTitle(false) == "Song 11");
        CHECK(a.getBpm() == 175.5 && a.getSource() == "Src" && a.getFavourites() == 9 && a.getPlays() == 1234 && a.hasVideo() && !a.hasStoryboard());
        CHECK(a.getStars() && a.getStars()->size() == 2 && (*a.getStars())[0] == 2.25 && (*a.getStars())[1] == 5.5);
        CHECK((*r)[1]->getDate() == "bad date" && (*r)[1]->getTitle(true) == "Song 22");
        CHECK(s.totalResults() == 2 + 20);
    }
    // ---- odd entries are SKIPPED (missing / wrong-typed field), the rest stay; total uses the raw array length ----
    {
        std::string noTitle = R"({"id":7,"artist":"A","creator":"C"})";
        std::string badId = R"({"id":"x","title":"T","artist":"A","creator":"C"})";
        std::string noCreator = R"({"id":8,"title":"T","artist":"A"})";
        fc.replies = {{200, "[" + item(1) + "," + noTitle + "," + item(2) + "," + badId + "," + noCreator + "," + item(3) + "]"}};
        auto r = s.resultList("q", 1, false);
        CHECK(r && r->size() == 3 && (*r)[0]->getID() == 1 && (*r)[1]->getID() == 2 && (*r)[2]->getID() == 3);
        CHECK(s.totalResults() == 6);                                // arr.length() (6), not the 3 kept
    }
    // ---- paging estimate: uses arr.length() >= 20 (not ==) ----
    {
        fc.replies = {{200, arrayOf(19)}}; s.resultList("q", 2, false); CHECK(s.totalResults() == 19 + 20);
        fc.replies = {{200, arrayOf(20)}}; s.resultList("q", 2, false); CHECK(s.totalResults() == 20 + 20 + 1);
        fc.replies = {{200, arrayOf(21)}}; s.resultList("q", 1, false); CHECK(s.totalResults() == 21 + 1);
        // 20 raw entries but all skipped: empty list yet the estimate still counts the 20
        std::string bad = "[";
        for (int i = 0; i < 20; i++) bad += (i ? "," : "") + std::string(R"({"id":1})");
        fc.replies = {{200, bad + "]"}};
        auto r = s.resultList("q", 1, false);
        CHECK(r && r->empty() && s.totalResults() == 21);
    }
    // ---- null paths reset the total to -1 ----
    {
        fc.replies = {{200, arrayOf(3)}}; s.resultList("q", 1, false); CHECK(s.totalResults() == 3);
        for (auto rep : std::vector<FakeClient::Reply>{{404, ""}, {200, "not json"}, {200, "{\"a\":1}"}}) {
            fc.replies = {rep};
            CHECK(!s.resultList("q", 1, false).has_value() && s.totalResults() == -1);
            fc.replies = {{200, arrayOf(3)}}; s.resultList("q", 1, false);
        }
    }
    // ---- Java quirk: a non-object element -> NullPointerException, uncaught; total keeps its value ----
    {
        fc.replies = {{200, arrayOf(4)}}; s.resultList("q", 1, false);
        for (std::string body : {std::string("[") + item(1) + ",5]", std::string("[") + item(1) + ",null]", std::string("[") + item(1) + ",[1]]"}) {
            fc.replies = {{200, body}};
            bool threw = false;
            try { s.resultList("q", 1, false); } catch (const std::logic_error&) { threw = true; }
            CHECK(threw && s.totalResults() == 4);
        }
    }
    // ---- IOException / no client propagate ----
    {
        fc.replies = {{200, "[]", true}};
        bool threw = false;
        try { s.resultList("q", 1, false); } catch (const java::IOException&) { threw = true; }
        CHECK(threw && s.totalResults() == 4);
        HinamizawaServer none;
        threw = false;
        try { none.resultList("q", 1, false); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw);
    }
    // ---- lookupSet: no set endpoint, goes straight to the search ----
    {
        fc.requests.clear();
        fc.replies = {{200, "[" + item(1) + "," + item(42) + "]"}};
        auto n = s.lookupSet(42, std::string(" some text "));
        CHECK(n && n->getID() == 42 && fc.requests.size() == 1);
        CHECK(fc.requests[0].url.toString() == BASE + "some+text&amount=20&offset=0");
        CHECK(s.lookupSet(42, std::nullopt) == nullptr && fc.requests.size() == 1);
    }
    {
        DownloadSource& src = s;
        CHECK(src.getDownloadURL(9) == std::optional<std::string>("https://mirror.hinamizawa.ai/api/v1/hinai/d/9"));
    }

    if (failures) { std::printf("batch27: %d FAILED\n", failures); return 1; }
    std::printf("batch27: all tests passed\n");
    return 0;
}
