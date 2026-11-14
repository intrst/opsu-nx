#pragma once
#include "opsu/downloads/DownloadServer.h"
#include "opsu/util/Json.h"

namespace opsu {

/**
 * Download server: https://mirror.nekoha.moe (API: https://github.com/mirror-nekoha-moe/mirror-server).
 * Port of itdelatrisu.opsu.downloads.servers.NekohaServer. The last of the 4 servers the Java build registers (DownloadsMenu).
 *
 * Differs from the other three (see DownloadServer.h for the shared seams, HinamizawaServer.h for the "skip odd entries" parser, which is the same here):
 *  - the API answers a JSON OBJECT (`readJsonObject`) whose `beatmapsets` array holds up to 100 sets per SERVER page; one UI page (20 sets)
 *    is a slice of it: `offset = (page - 1) * 20`, `apiPage = offset / 100 + 1`, `start = offset % 100` (Java truncating int division / remainder);
 *  - the last server page read is CACHED (key = the full search URL), so paging inside it does not touch the network;
 *  - status: RANKED -> `&status=ranked,approved`, LOVED -> `&status=loved`, anything else (ALL, 0, -1, ...) adds nothing; sort only for an empty query
 *    (`&sort=updated&order=desc` for ALL, `&sort=ranked&order=desc` otherwise);
 *  - total estimate = `offset + keptNodes + (more ? 1 : 0)`, `more` = slice ended before the array end or the array holds >= 100 sets;
 *  - set lookup endpoint `https://mirror.nekoha.moe/api/beatmapset/<id>`.
 *  - Java quirks kept: a non-object element (or a negative slice start, page 0 -> start -20, where optJSONObject(-20) is null) makes `parseItem(null)` throw a
 *    NullPointerException, uncaught: a `std::logic_error` here, the total keeps its value and the cache is NOT cleared.
 */
class NekohaServer : public DownloadServer {
public:
    explicit NekohaServer(HttpClient* client = nullptr) : DownloadServer(client) {}

    std::string getName() const override;
    std::optional<std::string> getDownloadURL(int beatmapSetID) override;
    std::optional<NodeList> resultList(const std::string& query, int page, bool rankedOnly) override;
    std::optional<NodeList> resultList(const std::string& query, int page, int status) override;
    int minQueryLength() override { return 0; }
    int totalResults() override { return totalResults_; }

    /** Sets per page returned by the server. */
    static constexpr int API_PAGE_SIZE = 100;
    /** Maximum beatmaps displayed per page (a server page is split into several of these). */
    static constexpr int PAGE_LIMIT = 20;

protected:
    std::optional<std::string> getLookupURL(int beatmapSetID) override;

private:
    /** Builds a result node from a beatmapset JSON object, or an empty pointer if it lacks the basics (Java: null). A null item throws like Java's NPE. */
    static std::unique_ptr<DownloadNode> parseItem(const json::JSONObject* item);

    /** The last server page that was read (Java: cachedKey / cachedSets). */
    std::optional<std::string> cachedKey_;
    std::optional<json::JSONArray> cachedSets_;
    /** Total result count from the last query. */
    int totalResults_ = -1;
};

}  // namespace opsu
