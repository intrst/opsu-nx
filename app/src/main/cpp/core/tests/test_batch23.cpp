// Tests for batch 23 (DownloadServer base class). Behavioural: a fake HttpClient and a fake server.
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include "opsu/downloads/DownloadServer.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// ---- fake HTTP ----
struct MemChannel : ReadableByteChannel {
    std::string data; size_t pos = 0; bool open_ = true; bool* closedFlag;
    MemChannel(std::string d, bool* cf) : data(std::move(d)), closedFlag(cf) {}
    int read(uint8_t* buf, std::size_t len) override {
        if (pos >= data.size()) return -1;
        size_t n = std::min<size_t>({len, data.size() - pos, 5});  // small chunks exercise the read loop
        std::memcpy(buf, data.data() + pos, n); pos += n; return static_cast<int>(n);
    }
    bool isOpen() const override { return open_; }
    void close() override { open_ = false; *closedFlag = true; }
};
struct FakeResponse : HttpResponse {
    int code; std::string text; Url u; MemChannel ch; bool* disconnected;
    FakeResponse(int c, std::string b, Url url, bool* cf, bool* dc) : code(c), text(b), u(std::move(url)), ch(b, cf), disconnected(dc) {}
    ~FakeResponse() override { *disconnected = true; }
    int statusCode() const override { return code; }
    const Url& url() const override { return u; }
    std::optional<std::string> header(const std::string&) const override { return std::nullopt; }
    int contentLength() const override { return static_cast<int>(text.size()); }
    ReadableByteChannel& body() override { return ch; }
};
struct FakeClient : HttpClient {
    struct Reply { int code; std::string body; bool throwIO = false; };
    std::deque<Reply> replies;
    std::vector<HttpRequest> requests;
    bool closed = false, disconnected = false;
    std::unique_ptr<HttpResponse> open(const HttpRequest& r) override {
        requests.push_back(r);
        Reply rep = replies.front(); replies.pop_front();
        if (rep.throwIO) throw java::IOException("timeout");
        return std::make_unique<FakeResponse>(rep.code, rep.body, r.url, &closed, &disconnected);
    }
};

// ---- fake server ----
struct TestServer : DownloadServer {
    using DownloadServer::DownloadServer;
    using DownloadServer::resultList;
    using DownloadServer::readJsonArray;
    using DownloadServer::readJsonObject;
    using DownloadServer::parseSet;
    using DownloadServer::applyStars;
    using DownloadServer::applySetInfo;
    std::optional<std::string> lookupUrl;
    bool lastRankedOnly = false; int searches = 0; std::string lastQuery; int lastPage = 0;
    NodeList toReturn; bool returnNull = false;
    std::string getName() const override { return "Test"; }
    std::optional<std::string> getDownloadURL(int id) override { return "http://x/" + std::to_string(id); }
    std::optional<NodeList> resultList(const std::string& q, int page, bool rankedOnly) override {
        searches++; lastRankedOnly = rankedOnly; lastQuery = q; lastPage = page;
        if (returnNull) return std::nullopt;
        NodeList out;
        for (auto& n : toReturn) out.push_back(std::move(n));
        toReturn.clear();
        return out;
    }
    int minQueryLength() override { return 0; }
    int totalResults() override { return -1; }
    std::optional<std::string> getLookupURL(int) override { return lookupUrl; }
};

static std::unique_ptr<DownloadNode> mkNode(int id) {
    return std::make_unique<DownloadNode>(id, "", "T", "", "A", "", "C");
}
static std::optional<std::string> label(int status) {
    DownloadNode n(1, "", "t", "", "a", "", "c"); n.setStatus(status); return n.getStatusLabel();
}

int main() {
    // ---- constants / small defaults ----
    CHECK(DownloadServer::STATUS_ALL == -99 && DownloadServer::STATUS_RANKED == 1 && DownloadServer::STATUS_LOVED == 4);
    TestServer s;
    CHECK(s.toString() == "Test" && s.getPreviewURL(123) == "https://b.ppy.sh/preview/123.mp3");
    CHECK(s.getPreviewURL(-5) == "https://b.ppy.sh/preview/-5.mp3");
    CHECK(s.getDownloadRequestHeaders().empty() && !s.isDownloadInBrowser() && !s.disableSSLInDownloads());
    DownloadSource& src = s;                                           // usable as a DownloadSource
    CHECK(src.getDownloadURL(7) == std::optional<std::string>("http://x/7"));

    // ---- resultList(status) default overload ----
    s.resultList("q", 3, DownloadServer::STATUS_ALL);  CHECK(!s.lastRankedOnly && s.lastPage == 3 && s.lastQuery == "q");
    s.resultList("q", 1, DownloadServer::STATUS_RANKED); CHECK(s.lastRankedOnly);
    s.resultList("q", 1, DownloadServer::STATUS_LOVED);  CHECK(s.lastRankedOnly);
    s.resultList("q", 1, 0);                             CHECK(s.lastRankedOnly);
    s.resultList("q", 1, false);                         CHECK(!s.lastRankedOnly);   // bool overload

    // ---- parseSet ----
    {
        json::JSONObject o(R"({"id":42,"title":"Song","artist":"Art","creator":"Map","last_updated":"2020-01-01",
          "ranked":4,"bpm":175.5,"source":"Src","favourite_count":9,"play_count":1234,"video":true,"storyboard":false,
          "beatmaps":[{"difficulty_rating":5.5,"version":"Hard","cs":4,"ar":9,"accuracy":8,"drain":6,"bpm":180,"total_length":120,
                       "count_circles":100,"count_sliders":50,"count_spinners":2},
                      {"difficulty_rating":2.25,"version":"Easy","cs":3,"od":5,"hp":4},
                      {"version":"no rating"}, {"difficulty_rating":-1}, 5, null]})");
        auto n = TestServer::parseSet(o);
        CHECK(n != nullptr && n->getID() == 42 && n->getTitle(false) == "Song" && n->getArtist(false) == "Art");
        CHECK(n->getCreator() == "Map" && n->getDate() == "2020-01-01");
        CHECK(n->getStatusLabel() == label(4));
        CHECK(n->getBpm() == 175.5 && n->getSource() == "Src" && n->getFavourites() == 9 && n->getPlays() == 1234);
        CHECK(n->hasVideo() && !n->hasStoryboard());
        CHECK(n->getStars().has_value() && n->getStars()->size() == 2 && (*n->getStars())[0] == 2.25 && (*n->getStars())[1] == 5.5);
        const auto& d = *n->getDifficulties();
        CHECK(d.size() == 2 && d[0].version == "Easy" && d[0].stars == 2.25 && d[0].cs == 3 && d[0].od == 5 && d[0].hp == 4);
        CHECK(d[0].ar == -1 && d[0].bpm == -1 && d[0].length == -1 && d[0].circles == -1 && d[0].sliders == -1 && d[0].spinners == -1);
        CHECK(d[1].version == "Hard" && d[1].cs == 4 && d[1].ar == 9 && d[1].od == 8 && d[1].hp == 6 && d[1].bpm == 180);
        CHECK(d[1].length == 120 && d[1].circles == 100 && d[1].sliders == 50 && d[1].spinners == 2);
    }
    {   // required fields / defaults
        CHECK(TestServer::parseSet(json::JSONObject(R"({"id":1,"artist":"a"})")) == nullptr);       // no title
        CHECK(TestServer::parseSet(json::JSONObject(R"({"id":1,"title":"t"})")) == nullptr);        // no artist
        CHECK(TestServer::parseSet(json::JSONObject(R"({"title":"t","artist":"a"})")) == nullptr);  // no id
        CHECK(TestServer::parseSet(json::JSONObject(R"({"id":"x","title":"t","artist":"a"})")) == nullptr);  // id not a number
        auto n = TestServer::parseSet(json::JSONObject(R"({"id":9,"title":"t","artist":"a"})"));
        CHECK(n && n->getCreator() == "" && n->getDate() == "" && !n->hasStars() && !n->getDifficulties().has_value());
        CHECK(n->getBpm() == -1 && n->getSource() == "" && n->getFavourites() == -1 && n->getPlays() == -1);
        CHECK(n->getStatusLabel() == label(-99));
    }
    {   // status words (raw osu! API) and precedence
        struct { const char* word; int status; } cases[] = {
            {"ranked", 1}, {"Ranked", 1}, {"RANKED", 1}, {"approved", 2}, {"qualified", 3}, {"loved", 4}, {"Loved", 4},
            {"pending", 0}, {"wip", -1}, {"WIP", -1}, {"graveyard", -2}, {"Graveyard", -2},
            {"ran\xE2\x84\xAA" "ed", 1},        // KELVIN SIGN folds to k in Java's equalsIgnoreCase
            {"pend\xC4\xB1ng", 0},              // dotless i folds to i
            {"ranked ", -99}, {" ranked", -99}, {"", -99}, {"rank", -99}, {"rankedd", -99}, {"nonsense", -99},
            {"ran\xE2\x84", -99},               // truncated UTF-8
        };
        for (auto& c : cases) {
            std::string js = std::string(R"({"id":1,"title":"t","artist":"a","status":")") + c.word + "\"}";
            auto n = TestServer::parseSet(json::JSONObject(js));
            CHECK(n != nullptr);
            if (n) CHECK(n->getStatusLabel() == label(c.status));
        }
        CHECK(label(1) != label(2) && label(2) != label(4) && label(0) != label(-2));  // the checks above are meaningful
        auto a = TestServer::parseSet(json::JSONObject(R"({"id":1,"title":"t","artist":"a","ranked":2,"status":"loved"})"));
        CHECK(a->getStatusLabel() == label(2));                       // numeric status wins
        auto b = TestServer::parseSet(json::JSONObject(R"({"id":1,"title":"t","artist":"a","ranked":-99,"status":"loved"})"));
        CHECK(b->getStatusLabel() == label(4));                       // -99 means "look at the word"
        auto c2 = TestServer::parseSet(json::JSONObject(R"({"id":1,"title":"t","artist":"a","ranked":3})"));
        CHECK(c2->getStatusLabel() == label(3));
    }

    // ---- applyStars / applySetInfo ----
    {
        DownloadNode n(1, "", "t", "", "a", "", "c");
        TestServer::applyStars(n, nullptr);
        json::JSONArray none(R"([{"version":"x"},{"difficulty_rating":-0.5},7,"s"])");
        TestServer::applyStars(n, &none);
        CHECK(!n.hasStars() && !n.getDifficulties().has_value());          // nothing usable: untouched
        json::JSONArray ties(R"([{"difficulty_rating":3,"version":"b"},{"difficulty_rating":1,"version":"a"},{"difficulty_rating":3,"version":"c"},{"difficulty_rating":0,"version":"z"}])");
        TestServer::applyStars(n, &ties);
        const auto& d = *n.getDifficulties();
        CHECK(d.size() == 4 && d[0].version == "z" && d[1].version == "a" && d[2].version == "b" && d[3].version == "c");   // stable
        CHECK(n.getMinStars() == 0 && n.getMaxStars() == 3);
        {   // many ties: std::sort would reorder equal ratings (it only keeps order below ~16 elements)
            DownloadNode m(2, "", "t", "", "a", "", "c");
            std::string js = "[";
            for (int i = 0; i < 90; i++) js += std::string(i ? "," : "") + "{\"difficulty_rating\":" + std::to_string((i * 7) % 3) + ",\"version\":\"" + std::to_string(i) + "\"}";
            json::JSONArray many(js + "]");
            TestServer::applyStars(m, &many);
            const auto& md = *m.getDifficulties();
            bool ordered = md.size() == 90;
            for (size_t i = 1; ordered && i < md.size(); i++)
                ordered = md[i - 1].stars < md[i].stars || (md[i - 1].stars == md[i].stars && std::stoi(md[i - 1].version) < std::stoi(md[i].version));
            CHECK(ordered);
        }
        TestServer::applySetInfo(n, nullptr);                             // no-op
        CHECK(n.getFavourites() == -1);
        json::JSONObject si(R"({"bpm":128,"source":"S","favourite_count":3,"play_count":4,"video":true,"storyboard":true})");
        TestServer::applySetInfo(n, &si);
        CHECK(n.getBpm() == 128 && n.getSource() == "S" && n.getFavourites() == 3 && n.getPlays() == 4 && n.hasVideo() && n.hasStoryboard());
    }

    // ---- readJsonArray / readJsonObject over the HttpClient seam ----
    {
        FakeClient fc; TestServer sv(&fc);
        Url u = Url::parse("http://host/path?q=1");
        fc.replies = {{200, "  \n[1, 2,3]\r\n\t"}};
        auto arr = sv.readJsonArray(u);
        CHECK(arr && arr->length() == 3);
        CHECK(fc.requests.size() == 1 && fc.requests[0].url.toString() == "http://host/path?q=1");
        CHECK(fc.requests[0].connectTimeoutMs == 10000 && fc.requests[0].readTimeoutMs == 15000 && !fc.requests[0].disableSSLCertValidation);
        CHECK(fc.requests[0].headers.size() == 2 && fc.requests[0].headers.at("User-Agent") == "opsu!" && fc.requests[0].headers.at("Accept") == "application/json");
        CHECK(fc.closed && fc.disconnected);
        const char* badArr[] = {"{}", "", "   ", "[1,", "\xEF\xBB\xBF[1]", "xyz", "null"};
        for (const char* b : badArr) { fc.disconnected = false; fc.replies = {{200, b}}; CHECK(!sv.readJsonArray(u)); CHECK(fc.disconnected); }
        fc.disconnected = false; fc.replies = {{404, "[1]"}}; CHECK(!sv.readJsonArray(u)); CHECK(fc.disconnected);
        fc.replies = {{201, "[1]"}}; CHECK(!sv.readJsonArray(u));          // only exactly 200 counts
        fc.replies = {{0, "", true}};
        bool threw = false; try { sv.readJsonArray(u); } catch (const java::IOException&) { threw = true; }
        CHECK(threw);

        fc.replies = {{200, R"({"id":5,"x":[1]})"}};
        auto o = sv.readJsonObject(u); CHECK(o && o->optInt("id", -1) == 5);
        fc.replies = {{200, R"( [{"id":6}, {"id":7}] )"}};
        o = sv.readJsonObject(u); CHECK(o && o->optInt("id", -1) == 6);    // first object of an array
        const char* badObj[] = {"[]", "[1]", "[null]", "xyz", "{", "[{", "", "\"str\""};
        for (const char* b : badObj) { fc.replies = {{200, b}}; CHECK(!sv.readJsonObject(u)); }
        fc.replies = {{500, "{}"}}; CHECK(!sv.readJsonObject(u));

        std::string big = "[";   // bigger than one 8192 read
        for (int i = 0; i < 3000; i++) big += (i ? ",{\"i\":" : "{\"i\":") + std::to_string(i) + "}";
        big += "]";
        fc.replies = {{200, big}};
        auto bigArr = sv.readJsonArray(u); CHECK(bigArr && bigArr->length() == 3000);

        TestServer noClient;
        threw = false; try { noClient.readJsonArray(u); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw);
    }

    // ---- lookupSet ----
    {
        FakeClient fc; TestServer sv(&fc);
        // no lookup URL, no search text -> nothing, no request
        CHECK(sv.lookupSet(5, std::nullopt) == nullptr && fc.requests.empty() && sv.searches == 0);
        CHECK(sv.lookupSet(5, std::string("  \t ")) == nullptr && sv.searches == 0);
        // fallback search: trimmed, page 1, status all (not ranked-only); picks the matching ID, skips null entries
        sv.toReturn.push_back(mkNode(1)); sv.toReturn.push_back(nullptr); sv.toReturn.push_back(mkNode(5));
        auto n = sv.lookupSet(5, std::string("  foo bar \n"));
        CHECK(n && n->getID() == 5 && sv.searches == 1 && sv.lastQuery == "foo bar" && sv.lastPage == 1 && !sv.lastRankedOnly);
        sv.toReturn.push_back(mkNode(1));
        CHECK(sv.lookupSet(5, std::string("foo")) == nullptr);                 // no result with this ID
        sv.returnNull = true;
        CHECK(sv.lookupSet(5, std::string("foo")) == nullptr);                 // null result list
        sv.returnNull = false;

        // set endpoint
        sv.lookupUrl = "http://host/set/5"; int searchesBefore = sv.searches;
        fc.replies = {{200, R"({"id":5,"title":"Via","artist":"Ep","ranked":1})"}};
        n = sv.lookupSet(5, std::string("ignored"));
        CHECK(n && n->getID() == 5 && n->getTitle(false) == "Via" && n->getStatusLabel() == label(1) && sv.searches == searchesBefore);
        CHECK(fc.requests.back().url.toString() == "http://host/set/5");
        // endpoint answers with another ID / missing fields / HTTP error -> fallback search
        fc.replies = {{200, R"({"id":6,"title":"Via","artist":"Ep"})"}}; sv.toReturn.push_back(mkNode(5));
        n = sv.lookupSet(5, std::string("q")); CHECK(n && n->getTitle(false) == "T" && sv.searches == searchesBefore + 1);
        fc.replies = {{200, R"({"id":5,"title":"Via"})"}}; sv.toReturn.push_back(mkNode(5));
        n = sv.lookupSet(5, std::string("q")); CHECK(n && n->getTitle(false) == "T");
        fc.replies = {{404, ""}};
        CHECK(sv.lookupSet(5, std::nullopt) == nullptr);
        fc.replies = {{200, R"([{"id":5,"title":"Arr","artist":"Ep"}])"}};          // array with one object is accepted
        n = sv.lookupSet(5, std::nullopt); CHECK(n && n->getTitle(false) == "Arr");
        // connection problems and bad lookup URLs propagate
        fc.replies = {{0, "", true}};
        bool threw = false; try { sv.lookupSet(5, std::string("q")); } catch (const java::IOException&) { threw = true; }
        CHECK(threw);
        sv.lookupUrl = "not a url";
        threw = false; try { sv.lookupSet(5, std::string("q")); } catch (const java::IOException&) { threw = true; }   // MalformedURLException
        CHECK(threw);
    }

    if (failures == 0) std::printf("batch23: all passed\n");
    return failures ? 1 : 0;
}
