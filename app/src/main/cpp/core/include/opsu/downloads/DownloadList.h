#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "opsu/OpsuConstants.h"
#include "opsu/downloads/Download.h"
#include "opsu/downloads/DownloadNode.h"

namespace opsu {

/**
 * Maintains the current downloads list. Port of itdelatrisu.opsu.downloads.DownloadList.
 *
 * Seams / deviations
 * - The list shares ownership of its nodes (`std::shared_ptr<DownloadNode>`): in Java the DownloadsMenu results and this list both
 *   referenced the same node. `getNode` returns nullptr for Java null; `getDownload` returns a raw pointer owned by the node.
 * - `get()` is the Java singleton; the constructor is public so tests (and a second list, if ever wanted) can make their own.
 * - `remove(node)` finds the node by identity (Java `indexOf` used `equals`, which DownloadNode never overrides).
 * - Not thread-safe, like the Java class (UI thread only).
 * - Java quirk kept: the id -> node map is keyed by beatmap set ID, so adding a second node with the same ID replaces the map entry
 *   while the list keeps both; removing either node later removes the map entry (even if it belongs to the other node).
 */
class DownloadList {
public:
    /** The exit confirmation message. */
    static const std::string& exitConfirmation() {
        static const std::string s =
            std::string("Beatmap downloads are in progress.\nAre you sure you want to quit ") + OpsuConstants::PROJECT_NAME + "?";
        return s;
    }

    /** Returns the single instance of this class. */
    static DownloadList& get();

    DownloadList() = default;
    DownloadList(const DownloadList&) = delete;
    DownloadList& operator=(const DownloadList&) = delete;

    /** The DownloadNode at an index, or nullptr if the index is out of bounds. */
    std::shared_ptr<DownloadNode> getNode(int index) const;
    /** The Download for a beatmap set ID, or nullptr if not in the list (or the node has no download). */
    Download* getDownload(int beatmapSetID) const;
    int size() const { return static_cast<int>(nodes.size()); }
    bool isEmpty() const { return nodes.empty(); }
    bool contains(int beatmapSetID) const { return map.count(beatmapSetID) != 0; }

    void addNode(std::shared_ptr<DownloadNode> node);
    void remove(const DownloadNode* node);
    /** Removes the node at the given index (out of bounds: nothing). */
    void remove(int index);

    /** True if the list contains any downloads that are active. */
    bool hasActiveDownloads() const;
    /** Cancels all (active) downloads. */
    void cancelAllDownloads();
    /** Removes all inactive downloads from the list (and clears their Download). */
    void clearInactiveDownloads();
    /** Removes all downloads with the given status from the list (and clears their Download). */
    void clearDownloads(Download::Status status);

private:
    std::vector<std::shared_ptr<DownloadNode>> nodes;
    std::map<int, std::shared_ptr<DownloadNode>> map;
};

}  // namespace opsu
