#pragma once
#include "opsu/downloads/DownloadServer.h"

namespace opsu {

/**
 * Download server: https://osu.direct (API: https://osu.direct/api/docs). Port of itdelatrisu.opsu.downloads.servers.OsuDirectServer.
 * One of the 4 servers the Java build registers (DownloadsMenu, SongInfoHeader).
 *
 * Seams / deviations (see DownloadServer.h for the shared ones)
 *  - `readJsonArray` goes through the `HttpClient` given to the constructor.
 *  - `ErrorHandler.error` is `DownloadServer::reportError` (only reachable for a MalformedURLException, which the built URL never causes).
 *  - Java returned the half-filled `DownloadNode[]` when a JSONException hit mid-list (unfilled elements null, the total result count keeps
 *    its previous value). Same here: the returned list has empty unique_ptr elements.
 *  - `optString(key, null)`: null / missing -> "" (DownloadNode treats null and empty Unicode names alike, see DownloadNode.h).
 *  - Dates are parsed as UTC and printed in the default time zone (the `TZ` environment variable), English month names.
 */
class OsuDirectServer : public DownloadServer {
public:
    explicit OsuDirectServer(HttpClient* client = nullptr) : DownloadServer(client) {}

    std::string getName() const override;
    std::optional<std::string> getDownloadURL(int beatmapSetID) override;
    std::optional<NodeList> resultList(const std::string& query, int page, bool rankedOnly) override;
    std::optional<NodeList> resultList(const std::string& query, int page, int status) override;
    int minQueryLength() override { return 0; }
    int totalResults() override { return totalResults_; }

    /** Maximum beatmaps displayed per page. */
    static constexpr int PAGE_LIMIT = 20;

protected:
    std::optional<std::string> getLookupURL(int beatmapSetID) override;

private:
    /** Total result count from the last query. */
    int totalResults_ = -1;
};

}  // namespace opsu
