// Tests for batch 26 (OsuDirectServer). Behavioural: a fake HttpClient.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string>
#include "opsu/downloads/servers/OsuDirectServer.h"

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
static const std::string BASE = "https://osu.direct/api/v2/search?mode=0&query=";

int main() {
    setenv("TZ", "UTC", 1);
    tzset();
    FakeClient fc;
    OsuDirectServer s(&fc);

    // ---- constants / small overrides ----
    CHECK(s.getName() == "osu.direct" && s.toString() == "osu.direct");
    CHECK(s.getDownloadURL(123) == std::optional<std::string>("https://osu.direct/api/d/123"));
    CHECK(s.getDownloadURL(-5) == std::optional<std::string>("https://osu.direct/api/d/-5"));
    CHECK(s.minQueryLength() == 0 && s.totalResults() == -1 && !s.disableSSLInDownloads() && !s.isDownloadInBrowser());
    CHECK(s.getDownloadRequestHeaders().empty() && OsuDirectServer::PAGE_LIMIT == 20);

    // ---- request URLs ----
    {
        fc.replies = {{200, "[]"}, {200, "[]"}, {200, "[]"}, {200, "[]"}, {200, "[]"}, {200, "[]"}, {200, "[]"}, {200, "[]"}};
        fc.requests.clear();
        s.resultList("foo bar&\xC3\xA9", 3, false);                                       // bool: false -> ALL
        s.resultList("abc", 1, true);                                                      // true -> RANKED
        s.resultList("", 1, DownloadServer::STATUS_ALL);
        s.resultList("abc", 2, DownloadServer::STATUS_RANKED);
        s.resultList("abc", 1, DownloadServer::STATUS_LOVED);
        s.resultList("abc", 1, 0);                                                         // any other int: ranked_date + &status=
        s.resultList("abc", 1, -1);
        s.resultList("abc", 0, false);                                                     // page 0: offset -20 (Java int arithmetic)
        CHECK(fc.requests.size() == 8);
        CHECK(fc.requests[0].url.toString() == BASE + "foo+bar%26%C3%A9&sort=last_updated%3Adesc&amount=20&offset=40");
        CHECK(fc.requests[1].url.toString() == BASE + "abc&sort=ranked_date%3Adesc&amount=20&offset=0&status=1");
        CHECK(fc.requests[2].url.toString() == BASE + "&sort=last_updated%3Adesc&amount=20&offset=0");
        CHECK(fc.requests[3].url.toString() == BASE + "abc&sort=ranked_date%3Adesc&amount=20&offset=20&status=1");
        CHECK(fc.requests[4].url.toString() == BASE + "abc&sort=ranked_date%3Adesc&amount=20&offset=0&status=4");
        CHECK(fc.requests[5].url.toString() == BASE + "abc&sort=ranked_date%3Adesc&amount=20&offset=0&status=0");
        CHECK(fc.requests[6].url.toString() == BASE + "abc&sort=ranked_date%3Adesc&amount=20&offset=0&status=-1");
        CHECK(fc.requests[7].url.toString() == BASE + "abc&sort=last_updated%3Adesc&amount=20&offset=-20");
        CHECK(s.totalResults() == -20);
        CHECK(fc.requests[0].connectTimeoutMs == 10000 && fc.requests[0].readTimeoutMs == 15000 && !fc.requests[0].disableSSLCertValidation);
        CHECK(fc.requests[0].headers.at("Accept") == "application/json" && fc.requests[0].headers.at("User-Agent") == "opsu!");
    }

    // ---- parsing: every field ----
    {
        std::string extra = R"("title_unicode":"\u66F2","artist_unicode":"\u30A2","ranked":4,"bpm":175.5,"source":"Src","favourite_count":9,"play_count":1234,)"
                            R"("video":true,"storyboard":false,"beatmaps":[{"difficulty_rating":5.5,"version":"Hard","cs":4,"ar":9,"accuracy":8,"drain":6},)"
                            R"({"difficulty_rating":2.25,"version":"Easy","cs":3}])";
        fc.replies = {{200, "[" + item(11, extra) + "," + item(22) + "]"}};
        auto r = s.resultList("query", 2, false);
        CHECK(r && r->size() == 2 && (*r)[0] && (*r)[1]);
        const DownloadNode& a = *(*r)[0];
        CHECK(a.getID() == 11 && a.getDate() == "14 May 2023 23:38:47" && a.getCreator() == "Map");
        CHECK(a.getTitle(false) == "Song 11" && a.getArtist(false) == "Art");
        CHECK(a.getTitle(true) == "\xE6\x9B\xB2" && a.getArtist(true) == "\xE3\x82\xA2");          // unicode names kept
        CHECK(a.getStatusLabel() == [&] { DownloadNode n(1, "", "t", "", "a", "", "c"); n.setStatus(4); return n.getStatusLabel(); }());
        CHECK(a.getBpm() == 175.5 && a.getSource() == "Src" && a.getFavourites() == 9 && a.getPlays() == 1234 && a.hasVideo() && !a.hasStoryboard());
        CHECK(a.getStars() && a.getStars()->size() == 2 && (*a.getStars())[0] == 2.25 && (*a.getStars())[1] == 5.5);
        CHECK(a.getDifficulties() && (*a.getDifficulties())[0].version == "Easy" && (*a.getDifficulties())[1].cs == 4);
        const DownloadNode& b = *(*r)[1];                                                         // minimal item: defaults
        CHECK(b.getID() == 22 && b.getTitle(true) == "Song 22" && !b.hasStars() && b.getBpm() == -1);
        CHECK(b.getStatusLabel() == [&] { DownloadNode n(1, "", "t", "", "a", "", "c"); n.setStatus(-99); return n.getStatusLabel(); }());
        CHECK(s.totalResults() == 2 + 20);
    }
    // unicode names: empty / equal to the plain name / JSON null / missing -> plain name is used
    {
        std::string body = "[" + item(1, R"("title_unicode":"","artist_unicode":"Art")") + "," + item(2, R"("title_unicode":"Song 2","artist_unicode":null)") + "," +
                           item(3, R"("title_unicode":null)") + "," + item(4, R"("title_unicode":"Other","artist_unicode":"Art2")") + "," +
                           item(5, R"("title_unicode":123)") + "]";
        fc.replies = {{200, body}};
        auto r = s.resultList("q", 1, false);
        CHECK(r && r->size() == 5);
        for (int i = 0; i < 3; i++) CHECK((*r)[i]->getTitle(true) == "Song " + std::to_string(i + 1) && (*r)[i]->getArtist(true) == "Art");
        CHECK((*r)[3]->getTitle(true) == "Other" && (*r)[3]->getArtist(true) == "Art2");
        CHECK((*r)[4]->getTitle(true) == "123");                                                  // optString converts a number
    }
    // dates
    {
        auto dateOf = [&](const std::string& raw) {
            fc.replies = {{200, "[" + item(1, "", raw) + "]"}};
            auto r = s.resultList("q", 1, false);
            return (*r)[0]->getDate();
        };
        CHECK(dateOf("2023-05-14T23:38:47Z") == "14 May 2023 23:38:47");
        CHECK(dateOf("2023-05-14T23:38:47.123456+02:00") == "14 May 2023 23:38:47");           // suffix dropped (first 19 units)
        CHECK(dateOf("2023-05-14T23:38:47") == "14 May 2023 23:38:47");
        CHECK(dateOf("2023-05-14T23:38:4") == "14 May 2023 23:38:04");                         // 18 chars: parsed as is
        CHECK(dateOf("2023-05-14T23:38:471") == "14 May 2023 23:38:47");                       // the 20th character is cut: "471" seconds would roll over
        CHECK(dateOf("2000-01-02T03:04:05Z") == "2 Jan 2000 03:04:05");
        CHECK(dateOf("xxxxxxxxxxxxxxxxxxxxxxxx") == "xxxxxxxxxxxxxxxxxxxxxxxx");              // >= 19 units, unparseable: the RAW string, not the prefix
        CHECK(dateOf("nope") == "nope" && dateOf("") == "");
        CHECK(dateOf("2023-05-14 23:38:47Z") == "2023-05-14 23:38:47Z");                       // space instead of 'T'
        // missing last_updated -> ""
        fc.replies = {{200, R"([{"id":1,"title":"t","artist":"a","creator":"c"}])"}};
        CHECK((*s.resultList("q", 1, false))[0]->getDate() == "");
        // parsed as UTC, printed in the default zone (UTC+7 here): the instant is preserved
        setenv("TZ", "WIB-7", 1);
        tzset();
        CHECK(dateOf("2023-05-14T23:38:47Z") == "15 May 2023 06:38:47");
        setenv("TZ", "UTC", 1);
        tzset();
    }
    // paging estimate
    {
        fc.replies = {{200, arrayOf(20)}};
        auto r = s.resultList("q", 1, false);
        CHECK(r && r->size() == 20 && s.totalResults() == 21 && (*r)[19]->getID() == 119);
        fc.replies = {{200, arrayOf(20)}}; s.resultList("q", 3, false); CHECK(s.totalResults() == 20 + 40 + 1);
        fc.replies = {{200, arrayOf(19)}}; s.resultList("q", 1, false); CHECK(s.totalResults() == 19);
        fc.replies = {{200, arrayOf(21)}}; s.resultList("q", 1, false); CHECK(s.totalResults() == 21);
        fc.replies = {{200, "[]"}}; auto e = s.resultList("q", 4, false); CHECK(e && e->empty() && s.totalResults() == 60);
    }
    // null results reset the total
    {
        fc.replies = {{200, arrayOf(3)}}; s.resultList("q", 1, false); CHECK(s.totalResults() == 3);
        fc.replies = {{404, "[]"}};
        CHECK(!s.resultList("q", 1, false).has_value() && s.totalResults() == -1);
        fc.replies = {{200, arrayOf(3)}}; s.resultList("q", 1, false);
        fc.replies = {{200, R"({"not":"an array"})"}};
        CHECK(!s.resultList("q", 1, false).has_value() && s.totalResults() == -1);
        fc.replies = {{200, arrayOf(3)}}; s.resultList("q", 1, false);
        fc.replies = {{200, "[{broken"}};
        CHECK(!s.resultList("q", 1, false).has_value() && s.totalResults() == -1);
    }
    // JSONException mid-list: half-filled list, total untouched
    {
        fc.replies = {{200, arrayOf(4)}}; s.resultList("q", 1, false);
        int before = s.totalResults();
        auto bad = [&](const std::string& element) {
            fc.replies = {{200, "[" + item(7) + "," + element + "," + item(8) + "]"}};
            auto r = s.resultList("q", 1, false);
            return r && r->size() == 3 && (*r)[0] && (*r)[0]->getID() == 7 && !(*r)[1] && !(*r)[2] && s.totalResults() == before;
        };
        CHECK(bad(R"({"id":5,"artist":"A","creator":"C"})"));                                  // no title
        CHECK(bad(R"({"id":5,"title":"T","creator":"C"})"));                                   // no artist
        CHECK(bad(R"({"title":"T","artist":"A","creator":"C"})"));                             // no id
        CHECK(bad(R"({"id":5,"title":"T","artist":"A"})"));                                    // no creator
        CHECK(bad(R"({"id":"x","title":"T","artist":"A","creator":"C"})"));                    // id not a number
        CHECK(bad(R"({"id":5,"title":7,"artist":"A","creator":"C"})"));                        // number is not a string for getString
        CHECK(bad("1"));                                                                       // not an object
    }
    // IOException / no client propagate and leave the total alone
    {
        fc.replies = {{200, arrayOf(5)}}; s.resultList("q", 1, false);
        fc.replies = {{200, "[]", true}};
        bool threw = false;
        try { s.resultList("q", 1, false); } catch (const java::IOException&) { threw = true; }
        CHECK(threw && s.totalResults() == 5);
        OsuDirectServer none;
        threw = false;
        try { none.resultList("q", 1, false); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw);
    }
    // ---- lookupSet: the set endpoint first, then a search ----
    {
        fc.requests.clear();
        fc.replies = {{200, item(42, R"("ranked":1)")}};
        auto n = s.lookupSet(42, std::string("ignored"));
        CHECK(n && n->getID() == 42 && fc.requests.size() == 1);
        CHECK(fc.requests[0].url.toString() == "https://osu.direct/api/v2/s/42");
        // endpoint answers with another set: falls back to the search and picks the matching id
        fc.requests.clear();
        fc.replies = {{200, item(43)}, {200, "[" + item(1) + "," + item(42) + "]"}};
        n = s.lookupSet(42, std::string(" some text "));
        CHECK(n && n->getID() == 42 && fc.requests.size() == 2);
        CHECK(fc.requests[1].url.toString() == BASE + "some+text&sort=last_updated%3Adesc&amount=20&offset=0");
        // endpoint fails, no search text: nothing
        fc.replies = {{404, ""}};
        CHECK(s.lookupSet(42, std::nullopt) == nullptr);
    }
    // usable as a DownloadSource
    {
        DownloadSource& src = s;
        CHECK(src.getDownloadURL(9) == std::optional<std::string>("https://osu.direct/api/d/9"));
    }

    if (failures) { std::printf("batch26: %d FAILED\n", failures); return 1; }
    std::printf("batch26: all tests passed\n");
    return 0;
}
