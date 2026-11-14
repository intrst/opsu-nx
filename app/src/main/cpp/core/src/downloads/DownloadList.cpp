#include "opsu/downloads/DownloadList.h"
#include <algorithm>

namespace opsu {

DownloadList& DownloadList::get() {
    static DownloadList list;
    return list;
}

std::shared_ptr<DownloadNode> DownloadList::getNode(int index) const {
    if (index < 0 || index >= size())
        return nullptr;
    return nodes[static_cast<size_t>(index)];
}

Download* DownloadList::getDownload(int beatmapSetID) const {
    auto it = map.find(beatmapSetID);
    return (it == map.end()) ? nullptr : it->second->getDownload();
}

void DownloadList::addNode(std::shared_ptr<DownloadNode> node) {
    nodes.push_back(node);
    map[node->getID()] = node;
}

void DownloadList::remove(const DownloadNode* node) {
    int idx = -1;
    for (size_t i = 0; i < nodes.size(); i++)
        if (nodes[i].get() == node) { idx = static_cast<int>(i); break; }
    remove(idx);
}

void DownloadList::remove(int index) {
    if (index < 0 || index >= size())
        return;
    std::shared_ptr<DownloadNode> node = nodes[static_cast<size_t>(index)];
    nodes.erase(nodes.begin() + index);
    map.erase(node->getID());
}

bool DownloadList::hasActiveDownloads() const {
    for (const auto& node : nodes) {
        const Download* dl = node->getDownload();
        if (dl != nullptr && dl->isActive())
            return true;
    }
    return false;
}

void DownloadList::cancelAllDownloads() {
    for (const auto& node : nodes) {
        Download* dl = node->getDownload();
        if (dl != nullptr && dl->isActive())
            dl->cancel();
    }
}

void DownloadList::clearInactiveDownloads() {
    for (auto it = nodes.begin(); it != nodes.end();) {
        Download* dl = (*it)->getDownload();
        if (dl != nullptr && !dl->isActive()) {
            std::shared_ptr<DownloadNode> node = *it;
            node->clearDownload();
            it = nodes.erase(it);
            map.erase(node->getID());
        } else {
            ++it;
        }
    }
}

void DownloadList::clearDownloads(Download::Status status) {
    for (auto it = nodes.begin(); it != nodes.end();) {
        Download* dl = (*it)->getDownload();
        if (dl != nullptr && dl->getStatus() == status) {
            std::shared_ptr<DownloadNode> node = *it;
            node->clearDownload();
            it = nodes.erase(it);
            map.erase(node->getID());
        } else {
            ++it;
        }
    }
}

}  // namespace opsu
