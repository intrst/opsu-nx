#include "opsu/downloads/DownloadServer.h"
#include "opsu/downloads/Download.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include "opsu/util/JavaMath.h"

namespace opsu {

namespace {

/** String.trim(): strips leading / trailing chars <= U+0020 (bytes <= 0x20 in UTF-8). */
std::string javaTrim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && static_cast<unsigned char>(s[b]) <= 0x20) b++;
    while (e > b && static_cast<unsigned char>(s[e - 1]) <= 0x20) e--;
    return s.substr(b, e - b);
}

/** Next code point of a UTF-8 string; a malformed / truncated sequence yields its lead byte alone (never matches a letter). */
uint32_t nextCodePoint(const std::string& s, size_t& i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    bool ok = extra > 0 && i + extra < s.size();
    for (size_t k = 1; ok && k <= extra; k++) ok = (static_cast<unsigned char>(s[i + k]) & 0xC0) == 0x80;
    if (!ok) { i++; return c; }
    uint32_t cp = c & (0x3Fu >> extra);
    for (size_t k = 1; k <= extra; k++) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3Fu);
    i += extra + 1;
    return cp;
}

/** String.equalsIgnoreCase(word) for an all-lowercase ASCII `word` (see the header for the 2 non-ASCII folds). */
bool equalsIgnoreCase(const std::string& s, const char* word) {
    size_t i = 0;
    const char* w = word;
    while (i < s.size()) {
        if (*w == '\0') return false;
        uint32_t cp = nextCodePoint(s, i);
        if (cp == 0x212A) cp = 'k';        // KELVIN SIGN lower-cases to 'k'
        else if (cp == 0x0131) cp = 'i';   // LATIN SMALL LETTER DOTLESS I upper-cases to 'I'
        else if (cp >= 'A' && cp <= 'Z') cp = cp - 'A' + 'a';
        if (cp != static_cast<uint32_t>(static_cast<unsigned char>(*w))) return false;
        w++;
    }
    return *w == '\0';
}

}  // namespace

std::optional<DownloadServer::NodeList> DownloadServer::resultList(const std::string& query, int page, int status) {
    return resultList(query, page, status != STATUS_ALL);
}

std::string DownloadServer::getPreviewURL(int beatmapSetID) {
    return "https://b.ppy.sh/preview/" + std::to_string(beatmapSetID) + ".mp3";
}

void DownloadServer::reportError(const std::string& msg, const std::exception* e, bool report) {
    Download::reportError(msg, e, report);
}

std::optional<std::string> DownloadServer::fetchBody(const Url& url) {
    if (client_ == nullptr) throw std::logic_error("DownloadServer: no HttpClient set");
    HttpRequest req;
    req.url = url;
    req.connectTimeoutMs = 10000;
    req.readTimeoutMs = 15000;
    req.headers["User-Agent"] = "opsu!";
    req.headers["Accept"] = "application/json";
    std::unique_ptr<HttpResponse> resp = client_->open(req);  // destroyed on every exit path = conn.disconnect()
    if (resp->statusCode() != 200) return std::nullopt;
    ReadableByteChannel& in = resp->body();
    std::string body;
    uint8_t buf[8192];
    int n;
    while ((n = in.read(buf, sizeof buf)) != -1) body.append(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
    in.close();
    return javaTrim(body);
}

std::optional<json::JSONArray> DownloadServer::readJsonArray(const Url& url) {
    std::optional<std::string> body = fetchBody(url);
    if (!body || body->empty() || (*body)[0] != '[') return std::nullopt;
    try {
        return json::JSONArray(*body);
    } catch (const json::JSONException&) {
        return std::nullopt;
    }
}

std::optional<json::JSONObject> DownloadServer::readJsonObject(const Url& url) {
    std::optional<std::string> body = fetchBody(url);
    if (!body || body->empty()) return std::nullopt;
    try {
        if ((*body)[0] == '{') return json::JSONObject(*body);
        if ((*body)[0] == '[') {
            json::JSONArray arr(*body);
            if (arr.length() > 0) {
                const json::JSONObject* first = arr.optJSONObject(0);
                if (first != nullptr) return *first;
            }
            return std::nullopt;
        }
        return std::nullopt;
    } catch (const json::JSONException&) {
        return std::nullopt;
    }
}

std::unique_ptr<DownloadNode> DownloadServer::lookupSet(int beatmapSetID, const std::optional<std::string>& searchText) {
    std::optional<std::string> url = getLookupURL(beatmapSetID);
    if (url) {
        std::optional<json::JSONObject> item = readJsonObject(Url::parse(*url));
        if (item && item->optInt("id", -1) == beatmapSetID) {
            std::unique_ptr<DownloadNode> node = parseSet(*item);
            if (node) return node;
        }
    }
    if (searchText && !javaTrim(*searchText).empty()) {
        std::optional<NodeList> results = resultList(javaTrim(*searchText), 1, STATUS_ALL);
        if (results) {
            for (std::unique_ptr<DownloadNode>& n : *results) {
                if (n && n->getID() == beatmapSetID) return std::move(n);
            }
        }
    }
    return nullptr;
}

std::unique_ptr<DownloadNode> DownloadServer::parseSet(const json::JSONObject& item) {
    try {
        std::string title = item.getString("title");
        std::string artist = item.getString("artist");
        int id = item.getInt("id");
        auto node = std::make_unique<DownloadNode>(id, item.optString("last_updated", ""), title, "", artist, "",
                                                   item.optString("creator", ""));
        applyStars(*node, item.optJSONArray("beatmaps"));
        applySetInfo(*node, &item);
        int status = item.optInt("ranked", -99);
        if (status == -99 && item.has("status")) {  // raw osu! API: status as a word
            std::string word = item.optString("status", "");
            if (equalsIgnoreCase(word, "ranked")) status = 1;
            else if (equalsIgnoreCase(word, "approved")) status = 2;
            else if (equalsIgnoreCase(word, "qualified")) status = 3;
            else if (equalsIgnoreCase(word, "loved")) status = 4;
            else if (equalsIgnoreCase(word, "pending")) status = 0;
            else if (equalsIgnoreCase(word, "wip")) status = -1;
            else if (equalsIgnoreCase(word, "graveyard")) status = -2;
        }
        node->setStatus(status);
        return node;
    } catch (const json::JSONException&) {
        return nullptr;
    }
}

void DownloadServer::applyStars(DownloadNode& node, const json::JSONArray* beatmaps) {
    if (beatmaps == nullptr) return;
    std::vector<DownloadNode::Difficulty> list;
    for (int i = 0; i < beatmaps->length(); i++) {
        const json::JSONObject* bm = beatmaps->optJSONObject(i);
        if (bm == nullptr || !bm->has("difficulty_rating")) continue;
        double sr = bm->optDouble("difficulty_rating", -1);
        if (sr < 0) continue;
        DownloadNode::Difficulty d;
        d.stars = sr;
        d.version = bm->optString("version", "");
        d.cs = bm->optDouble("cs", -1);
        d.ar = bm->optDouble("ar", -1);
        d.od = bm->has("accuracy") ? bm->optDouble("accuracy", -1) : bm->optDouble("od", -1);
        d.hp = bm->has("drain") ? bm->optDouble("drain", -1) : bm->optDouble("hp", -1);
        d.bpm = bm->optDouble("bpm", -1);
        d.length = bm->optInt("total_length", -1);
        d.circles = bm->optInt("count_circles", -1);
        d.sliders = bm->optInt("count_sliders", -1);
        d.spinners = bm->optInt("count_spinners", -1);
        list.push_back(std::move(d));
    }
    if (list.empty()) return;
    // Collections.sort is stable; Double.compare orders -0.0 < 0.0 and NaN last
    std::stable_sort(list.begin(), list.end(), [](const DownloadNode::Difficulty& a, const DownloadNode::Difficulty& b) {
        return java::compare(a.stars, b.stars) < 0;
    });
    std::vector<double> stars(list.size());
    for (size_t i = 0; i < stars.size(); i++) stars[i] = list[i].stars;
    node.setStars(std::move(stars));
    node.setDifficulties(std::move(list));
}

void DownloadServer::applySetInfo(DownloadNode& node, const json::JSONObject* item) {
    if (item == nullptr) return;
    node.setSetInfo(item->optDouble("bpm", -1), item->optString("source", ""), item->optInt("favourite_count", -1),
                    item->optInt("play_count", -1), item->optBoolean("video", false),
                    item->optBoolean("storyboard", false));
}

}  // namespace opsu
