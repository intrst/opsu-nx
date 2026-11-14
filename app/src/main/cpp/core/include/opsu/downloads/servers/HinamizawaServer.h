#pragma once
#include "opsu/downloads/DownloadServer.h"

namespace opsu {

/**
 * Download server: https://hinamizawa.ai/osu/beatmaps/ (the "hinai" mirror; API: https://mirror.hinamizawa.ai/docs/beatmap-search).
 * Port of itdelatrisu.opsu.downloads.servers.HinamizawaServer. One of the 4 servers the Java build registers (DownloadsMenu).
 *
 * Differs from CatboyServer / OsuDirectServer (see DownloadServer.h for the shared seams):
 *  - search URL uses `q=` (not `query=`), has no `sort` parameter unless the query is empty (then `&sort=updated:desc` / `&sort=ranked_date:desc`, not URL-encoded);
 *  - NO set lookup endpoint (`getLookupURL` is the base default, so `lookupSet` goes straight to the search fallback);
 *  - an entry that lacks the basics (missing / wrong-typed field) is SKIPPED, the page is not cut short; the total estimate uses the raw array length (`>= PAGE_LIMIT`);
 *  - the response is parsed with no mid-list JSONException path, so the half-filled-list behaviour of the other two servers does not exist here.
 *  - Java quirk kept: an array element that is not a JSON object makes `parseItem(null)` throw a NullPointerException, which `resultList` does not catch;
 *    here it is a `std::logic_error` that propagates (the total result count keeps its previous value).
 */
class HinamizawaServer : public DownloadServer {
public:
    explicit HinamizawaServer(HttpClient* client = nullptr) : DownloadServer(client) {}

    std::string getName() const override;
    std::optional<std::string> getDownloadURL(int beatmapSetID) override;
    std::optional<NodeList> resultList(const std::string& query, int page, bool rankedOnly) override;
    std::optional<NodeList> resultList(const std::string& query, int page, int status) override;
    int minQueryLength() override { return 0; }
    int totalResults() override { return totalResults_; }

    /** Maximum beatmaps displayed per page. */
    static constexpr int PAGE_LIMIT = 20;

private:
    /** Builds a result node from a beatmapset JSON object, or an empty pointer if it lacks the basics (Java: null). A null item throws like Java's NPE. */
    static std::unique_ptr<DownloadNode> parseItem(const json::JSONObject* item);

    /** Total result count from the last query. */
    int totalResults_ = -1;
};

}  // namespace opsu
