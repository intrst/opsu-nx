#include "opsu/downloads/servers/OsuDirectServer.h"
#include <cstdint>
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

std::string OsuDirectServer::getName() const { return "osu.direct"; }

std::optional<std::string> OsuDirectServer::getLookupURL(int beatmapSetID) {
    return "https://osu.direct/api/v2/s/" + std::to_string(beatmapSetID);
}

std::optional<std::string> OsuDirectServer::getDownloadURL(int beatmapSetID) {
    return "https://osu.direct/api/d/" + std::to_string(beatmapSetID);
}

std::optional<DownloadServer::NodeList> OsuDirectServer::resultList(const std::string& query, int page, bool rankedOnly) {
    return resultList(query, page, rankedOnly ? STATUS_RANKED : STATUS_ALL);
}

std::optional<DownloadServer::NodeList> OsuDirectServer::resultList(const std::string& query, int page, int status) {
    std::optional<NodeList> nodes;  // Java: null
    try {
        // build request: with no query, show the newest maps first
        int offset = (int)((uint32_t)(page - 1) * (uint32_t)PAGE_LIMIT);  // Java int arithmetic wraps
        std::string sort = (status == STATUS_ALL) ? "last_updated:desc" : "ranked_date:desc";
        std::string search = "https://osu.direct/api/v2/search?mode=0&query=" + java::urlEncode(query) +
                             "&sort=" + java::urlEncode(sort) + "&amount=" + std::to_string(PAGE_LIMIT) +
                             "&offset=" + std::to_string(offset);
        if (status != STATUS_ALL)
            search += "&status=" + std::to_string(status);

        // read JSON
        std::optional<json::JSONArray> arr = readJsonArray(Url::parse(search));
        if (!arr) {
            totalResults_ = -1;
            return std::nullopt;
        }

        // parse result list
        nodes = NodeList();
        nodes->resize(static_cast<size_t>(arr->length()));
        for (int i = 0; i < arr->length(); i++) {
            const json::JSONObject& item = arr->getJSONObject(i);
            // same order as the Java statements / argument list (a missing field stops the loop at this element)
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
            (*nodes)[static_cast<size_t>(i)] = std::move(node);
        }

        // no total count in the API: estimate so that paging works
        int resultCount = (int)((uint32_t)nodes->size() + (uint32_t)offset);
        if ((int)nodes->size() == PAGE_LIMIT)
            resultCount++;
        totalResults_ = resultCount;
    } catch (const MalformedURLException& e) {
        reportError("Problem loading result list for query '" + query + "'.", &e, true);
        Log::error(e);
    } catch (const json::JSONException& e) {
        Log::error(e);
    }
    return nodes;
}

}  // namespace opsu
