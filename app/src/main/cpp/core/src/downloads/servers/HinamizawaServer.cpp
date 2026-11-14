#include "opsu/downloads/servers/HinamizawaServer.h"
#include <cstdint>
#include <stdexcept>
#include "opsu/logging/Log.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaDate.h"
#include "opsu/util/JavaIO.h"

namespace opsu {

namespace {
/**
 * "2023-05-14T23:38:47Z" -> "14 May 2023 23:38:47". Drops fractional seconds / a time zone suffix (only the first 19 UTF-16 units are
 * parsed), treats the rest as UTC and prints in the default time zone. The raw string if it cannot be parsed.
 */
std::string formatDate(const std::string& s) {
    std::string base = s;
    if (java::utf16Length(s) >= 19) {  // s.substring(0, 19) in UTF-16 units: whole code points only (a cut surrogate pair cannot parse either way)
        size_t units = 0, i = 0;
        while (i < s.size()) {
            unsigned char c = (unsigned char)s[i];
            size_t len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            size_t u = len == 4 ? 2 : 1;
            if (units + u > 19) break;
            units += u;
            i += len;
        }
        base = s.substr(0, i);
    }
    std::optional<int64_t> ms = java::parseDate(base, "yyyy-MM-dd'T'HH:mm:ss", true);
    return ms ? java::formatDate(*ms) : s;  // ParseException: the raw string (not `base`)
}

/** `item.optString(key, null)`: null for a missing key or JSON null, else the value as text. */
bool presentString(const json::JSONObject& item, const std::string& key, std::string& out) {
    if (!item.has(key) || item.isNull(key)) return false;
    out = item.optString(key, "");
    return true;
}
}  // namespace

std::string HinamizawaServer::getName() const { return "Hinamizawa"; }

std::optional<std::string> HinamizawaServer::getDownloadURL(int beatmapSetID) {
    return "https://mirror.hinamizawa.ai/api/v1/hinai/d/" + std::to_string(beatmapSetID);
}

std::unique_ptr<DownloadNode> HinamizawaServer::parseItem(const json::JSONObject* item_) {
    if (!item_) throw std::logic_error("NullPointerException: beatmapset entry is not a JSON object");  // Java: item.getString on null
    const json::JSONObject& item = *item_;
    try {
        std::string title = item.getString("title");
        std::string artist = item.getString("artist");
        std::string titleU, artistU;
        bool hasTitleU = presentString(item, "title_unicode", titleU);
        bool hasArtistU = presentString(item, "artist_unicode", artistU);
        if (hasTitleU && (titleU.empty() || titleU == title))
            titleU.clear();
        if (hasArtistU && (artistU.empty() || artistU == artist))
            artistU.clear();
        int id = item.getInt("id");
        std::string date = formatDate(item.optString("last_updated", ""));
        std::string creator = item.getString("creator");
        std::unique_ptr<DownloadNode> node = std::make_unique<DownloadNode>(id, date, title, titleU, artist, artistU, creator);
        applyStars(*node, item.optJSONArray("beatmaps"));
        applySetInfo(*node, &item);
        node->setStatus(item.optInt("ranked", -99));
        return node;
    } catch (const json::JSONException&) {
        return nullptr;
    }
}

std::optional<DownloadServer::NodeList> HinamizawaServer::resultList(const std::string& query, int page, bool rankedOnly) {
    return resultList(query, page, rankedOnly ? STATUS_RANKED : STATUS_ALL);
}

std::optional<DownloadServer::NodeList> HinamizawaServer::resultList(const std::string& query, int page, int status) {
    std::optional<NodeList> nodes;  // Java: null
    try {
        // build request: with no query, show the newest maps first (a text query keeps the server's relevance order)
        int offset = (int)((uint32_t)(page - 1) * (uint32_t)PAGE_LIMIT);  // Java int arithmetic wraps
        std::string search = "https://mirror.hinamizawa.ai/api/v2/search?mode=0&q=" + java::urlEncode(query) +
                             "&amount=" + std::to_string(PAGE_LIMIT) + "&offset=" + std::to_string(offset);
        if (status != STATUS_ALL)
            search += "&status=" + std::to_string(status);
        if (query.empty())
            search += (status == STATUS_ALL) ? "&sort=updated:desc" : "&sort=ranked_date:desc";

        // read JSON
        std::optional<json::JSONArray> arr = readJsonArray(Url::parse(search));
        if (!arr) {
            totalResults_ = -1;
            return std::nullopt;
        }

        // parse result list (an odd entry is skipped instead of failing the whole page)
        NodeList list;
        for (int i = 0; i < arr->length(); i++) {
            std::unique_ptr<DownloadNode> node = parseItem(arr->optJSONObject(i));
            if (node)
                list.push_back(std::move(node));
        }
        nodes = std::move(list);

        // no total count on this path: estimate so that paging works
        int resultCount = (int)((uint32_t)arr->length() + (uint32_t)offset);
        if (arr->length() >= PAGE_LIMIT)
            resultCount++;
        totalResults_ = resultCount;
    } catch (const MalformedURLException& e) {
        reportError("Problem loading result list for query '" + query + "'.", &e, true);
        Log::error(e);
    }
    return nodes;
}

}  // namespace opsu
