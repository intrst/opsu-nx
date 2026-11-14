#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "opsu/downloads/DownloadNode.h"
#include "opsu/downloads/HttpClient.h"
#include "opsu/util/Json.h"

namespace opsu {

/**
 * Abstract base class for beatmap download servers. Port of itdelatrisu.opsu.downloads.servers.DownloadServer
 * (the base class only: none of the 11 concrete servers is ported yet).
 *
 * It implements `DownloadSource` (the three methods DownloadNode uses), so a server can be handed straight to
 * `DownloadNode::createDownload`.
 *
 * Seams / deviations
 *  - Java's static `readJsonArray(URL)` / `readJsonObject(URL)` opened a `HttpURLConnection` themselves. Here they are
 *    PROTECTED MEMBERS that go through the `HttpClient` the server was constructed with (non-owning, must outlive the
 *    server; nullptr is allowed until a request is made, then std::logic_error). Same request: connect timeout 10 s, read
 *    timeout 15 s, `User-Agent: opsu!`, `Accept: application/json`; status != 200 -> nullopt; connection problems
 *    (java::IOException) propagate; the response is disconnected when it goes out of scope (Java: `finally`).
 *  - The body is used as raw UTF-8 bytes. Java decoded it with `new String(bytes, "UTF-8")`, which turns invalid
 *    sequences into U+FFFD; here they stay as they are. JSON from the servers is valid UTF-8, so this only matters for a
 *    broken server. `String.trim()` (strip chars <= U+0020) is a byte trim here, which is identical for UTF-8.
 *  - Nullable results: `DownloadNode[]` (null array, and null elements) is `std::optional<NodeList>` with possibly-null
 *    unique_ptr elements; a null `DownloadNode` is an empty unique_ptr; `String` URL / search text is std::optional<std::string>;
 *    a `JSONObject` / `JSONArray` result is std::optional (null = nullopt).
 *  - `resultList` has two overloads (Java: `boolean rankedOnly` and `int status`). C++ would hide the inherited one when a
 *    subclass overrides the other, so subclasses must write `using DownloadServer::resultList;`. Literals / variables
 *    still pick the right one (`true` -> bool, `STATUS_ALL` -> int).
 *  - `equalsIgnoreCase` for the status words is ASCII case-insensitive plus the two non-ASCII characters Java folds into
 *    ASCII letters that occur in the words ("K" Kelvin sign U+212A -> k, dotless i U+0131 -> i); no other character can match.
 *  - `getName()` is const; `toString()` = getName().
 */
class DownloadServer : public DownloadSource {
public:
    /** Status filter: show all maps. */
    static constexpr int STATUS_ALL = -99;
    /** Status filter: ranked maps only (osu! API ranked status 1). */
    static constexpr int STATUS_RANKED = 1;
    /** Status filter: loved maps only (osu! API ranked status 4). */
    static constexpr int STATUS_LOVED = 4;

    using NodeList = std::vector<std::unique_ptr<DownloadNode>>;

    explicit DownloadServer(HttpClient* client = nullptr) : client_(client) {}
    ~DownloadServer() override = default;

    /** The name of the download server. */
    virtual std::string getName() const = 0;

    /** A web address to download the given beatmap; DownloadSource::getDownloadURL (nullopt = could not be determined). */
    std::optional<std::string> getDownloadURL(int beatmapSetID) override = 0;

    /**
     * Results for a search query, or nullopt if the list could not be created.
     * @throws java::IOException if any connection problem occurs
     */
    virtual std::optional<NodeList> resultList(const std::string& query, int page, bool rankedOnly) = 0;

    /**
     * Results for a search query and status filter. Servers that support status filtering override this; the default
     * falls back to the `rankedOnly` overload (anything but "all" means ranked only).
     */
    virtual std::optional<NodeList> resultList(const std::string& query, int page, int status);

    /**
     * Looks up a single beatmap set by its ID (the song menu uses it to show the ranked status). First asks the set
     * endpoint (`getLookupURL`); if that gives nothing, falls back to a normal search for `searchText` and picks the
     * result with the same ID.
     * @return the node, or an empty pointer if the server does not know the set
     */
    virtual std::unique_ptr<DownloadNode> lookupSet(int beatmapSetID, const std::optional<std::string>& searchText);

    /** Minimum allowable length of a search query (0 if none). */
    virtual int minQueryLength() = 0;

    /** Total number of results for the last search query (-1 if no query). */
    virtual int totalResults() = 0;

    /** A web address to preview the given beatmap ("https://b.ppy.sh/preview/%d.mp3"). */
    virtual std::string getPreviewURL(int beatmapSetID);

    /** Extra HTTP request headers for the download request (empty = Java null). */
    std::map<std::string, std::string> getDownloadRequestHeaders() override { return {}; }

    /** Whether downloads must be made through a web browser. */
    virtual bool isDownloadInBrowser() { return false; }

    /** Whether SSL certificate validation should be disabled for downloads. */
    bool disableSSLInDownloads() override { return false; }

    /** Java toString(): the server name. */
    std::string toString() const { return getName(); }

protected:
    /** ErrorHandler.error(msg, e, report) (forwards to Download::reportError). */
    static void reportError(const std::string& msg, const std::exception* e, bool report);

    /** A web address that returns a single beatmap set (JSON) for the set ID; nullopt if the server has no such endpoint. */
    virtual std::optional<std::string> getLookupURL(int /*beatmapSetID*/) { return std::nullopt; }

    /** Reads a JSON array from a URL; nullopt if the request failed or the response was not an array. */
    std::optional<json::JSONArray> readJsonArray(const Url& url);

    /** Reads a JSON object from a URL (an array holding one object is accepted too); nullopt if it failed / was no object. */
    std::optional<json::JSONObject> readJsonObject(const Url& url);

    /** Builds a node from a beatmap set JSON object (the same fields the search results use); empty if required fields are missing. */
    static std::unique_ptr<DownloadNode> parseSet(const json::JSONObject& item);

    /**
     * Reads the difficulties from a "beatmaps" JSON array and stores them on the node: the star ratings (sorted, lowest
     * first) and the full per-difficulty details (sorted by stars). `beatmaps` may be null (nothing happens).
     */
    static void applyStars(DownloadNode& node, const json::JSONArray* beatmaps);

    /** Reads the set-level details (BPM, source, favourites, plays, video / storyboard) and stores them on the node. */
    static void applySetInfo(DownloadNode& node, const json::JSONObject* item);

private:
    HttpClient* client_;

    /** GET with the Java request settings; returns the trimmed body, or nullopt for a status other than 200. */
    std::optional<std::string> fetchBody(const Url& url);
};

}  // namespace opsu
