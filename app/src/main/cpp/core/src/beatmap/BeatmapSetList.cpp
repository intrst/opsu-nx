#include "opsu/beatmap/BeatmapSetList.h"
#include <algorithm>
#include <random>
#include <stdexcept>
#include "opsu/beatmap/BeatmapGroup.h"
#include "opsu/beatmap/BeatmapSearch.h"
#include "opsu/beatmap/BeatmapSortOrder.h"
#include "opsu/downloads/Download.h"
#include "opsu/util/JavaIO.h"

namespace opsu {

namespace {
double defaultRandom() {
    static std::mt19937_64 rng{std::random_device{}()};
    return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
}
}  // namespace

BeatmapSetList::BeatmapSetList() { reset(); }

void BeatmapSetList::reset() {
    if (currentGroup() == BeatmapGroup::ALL) {
        groupNodes_ = parsedNodes_;  // Java: BeatmapGroup.ALL.filter returns its argument, the very same list
    } else {
        groupNodes_ = std::make_shared<NodeList>(filter(currentGroup(), *parsedNodes_, [](BeatmapSetNode* n) -> const BeatmapSetBeatmaps& {
            return n->getBeatmapSet().beatmaps();
        }));
    }
    nodes_ = groupNodes_;
    expandedIndex_ = -1;
    expandedStart_ = expandedEnd_ = nullptr;
    lastQuery_.reset();
}

BeatmapSetNode* BeatmapSetList::addSongGroup(std::vector<Beatmap> beatmaps, const Beatmap::ModParams& current) {
    if (beatmaps.empty()) throw std::out_of_range("BeatmapSetList::addSongGroup: empty beatmap list");
    int count = static_cast<int>(beatmaps.size());
    ownedNodes_.push_back(std::make_unique<BeatmapSetNode>(BeatmapSet(std::move(beatmaps), current)));
    BeatmapSetNode* node = ownedNodes_.back().get();
    parsedNodes_->push_back(node);  // with the group ALL this is also `nodes` (same list), so size() grows at once, as in Java
    mapCount_ += count;
    // Java: `beatmaps.get(0)` AFTER the BeatmapSet constructor sorted that very list in place, i.e. the easiest difficulty
    int msid = node->getBeatmapSet().get(0).beatmapSetID;
    if (msid > 0) msidDb_.insert(msid);
    // add MD5 hashes to the table (Java iterates the same, already sorted list; a duplicate hash is overwritten by the later beatmap)
    for (const Beatmap& b : node->getBeatmapSet())
        if (!b.md5Hash.empty()) beatmapHashDb_[b.md5Hash] = HashEntry{node->beatmapSetPtr(), b.getFile()};
    return node;
}

const Beatmap* BeatmapSetList::getBeatmapFromHash(const std::string& beatmapHash) const {
    auto it = beatmapHashDb_.find(beatmapHash);
    if (it == beatmapHashDb_.end()) return nullptr;
    for (const Beatmap& b : *it->second.set)
        if (b.getFile() == it->second.file) return &b;
    return nullptr;  // the beatmap left its set without the entry being erased (deleteSong will erase it)
}

BeatmapSetNode* BeatmapSetList::getBaseNode(int index) const {
    if (index < 0 || index >= size()) return nullptr;
    return (*nodes_)[static_cast<std::size_t>(index)];
}

BeatmapSetNode* BeatmapSetList::getRandomNode() {
    double r = random_ ? random_() : defaultRandom();
    BeatmapSetNode* node = getBaseNode(static_cast<int>(r * size()));
    if (node && node->index == expandedIndex_) node = node->next;  // don't choose an expanded group node
    return node;
}

BeatmapSetNode* BeatmapSetList::getNode(BeatmapSetNode* node, int shift) const {
    BeatmapSetNode* start = node;
    if (shift > 0) {
        for (int i = 0; i < shift && start; i++) start = start->next;
    } else {
        for (int i = 0; i < shift && start; i++) start = start->prev;  // never runs for shift <= 0 (Java quirk)
    }
    return start;
}

void BeatmapSetList::retireExpansion() {
    if (expanded_.empty()) return;
    retired_.push_back(std::move(expanded_));
    expanded_.clear();
    while (retired_.size() > RETIRED_EXPANSIONS) retired_.pop_front();
}

BeatmapSetNode* BeatmapSetList::expand(int index) {
    unexpand();  // undo the previous expansion
    BeatmapSetNode* node = getBaseNode(index);
    if (!node) return nullptr;
    retireExpansion();  // the previous expansion's nodes are no longer linked; keep them alive a while

    expandedStart_ = expandedEnd_ = nullptr;
    BeatmapSetNode* prevNode = node->prev;
    BeatmapSetNode* nextNode = node->next;
    const std::shared_ptr<BeatmapSet>& set = node->beatmapSetPtr();
    for (int i = 0, size = static_cast<int>(set->size()); i < size; i++) {
        expanded_.push_back(std::make_unique<BeatmapSetNode>(set));
        BeatmapSetNode* newNode = expanded_.back().get();
        newNode->index = index;
        newNode->beatmapIndex = i;
        newNode->prev = node;
        if (i == 0) {  // unlink the group node
            expandedStart_ = newNode;
            newNode->prev = prevNode;
            if (prevNode) prevNode->next = newNode;
        }
        node->next = newNode;
        node = node->next;
    }
    if (nextNode) {
        node->next = nextNode;
        nextNode->prev = node;
    }
    expandedEnd_ = node;
    expandedIndex_ = index;
    return expandedStart_;
}

void BeatmapSetList::unexpand() {
    if (expandedIndex_ < 0 || expandedIndex_ >= size()) return;
    // recreate surrounding links
    BeatmapSetNode* ePrev = getBaseNode(expandedIndex_ - 1);
    BeatmapSetNode* eCur = getBaseNode(expandedIndex_);
    BeatmapSetNode* eNext = getBaseNode(expandedIndex_ + 1);
    if (ePrev) ePrev->next = eCur;
    eCur->prev = ePrev;
    eCur->index = expandedIndex_;
    eCur->next = eNext;
    if (eNext) eNext->prev = eCur;
    expandedIndex_ = -1;
    expandedStart_ = expandedEnd_ = nullptr;
}

void BeatmapSetList::init() {
    if (size() < 1) return;
    BeatmapSortOrder order = current();
    // Java sorts `nodes`: after a search only the result list, never groupNodes (with the group ALL and no search it is parsedNodes too)
    std::stable_sort(nodes_->begin(), nodes_->end(), [&](BeatmapSetNode* a, BeatmapSetNode* b) {
        return less(order, a->getBeatmapSet().beatmaps(), b->getBeatmapSet().beatmaps());
    });
    expandedIndex_ = -1;  // Java clears the expansion here without re-linking
    expandedStart_ = expandedEnd_ = nullptr;
    retireExpansion();

    BeatmapSetNode* last = (*nodes_)[0];
    last->index = 0;
    last->prev = nullptr;
    for (int i = 1, n = size(); i < n; i++) {
        BeatmapSetNode* node = (*nodes_)[static_cast<std::size_t>(i)];
        last->next = node;
        node->index = i;
        node->prev = last;
        last = node;
    }
    last->next = nullptr;
}

bool BeatmapSetList::applySearch(const std::string& normalizedQuery, const SearchSnapshot& snapshot, std::vector<BeatmapSetNode*> result) {
    if (snapshot.owner_ != groupNodes_ || snapshot.size_ != groupNodes_->size()) return false;  // the list changed since the snapshot
    lastQuery_ = normalizedQuery;
    nodes_ = std::make_shared<NodeList>(std::move(result));
    return true;
}

bool BeatmapSetList::search(const std::optional<std::string>& query) {
    if (!query) return false;
    std::string q = normalizeQuery(query);
    if (lastQuery_ && q == *lastQuery_) return false;  // don't redo the same search
    lastQuery_ = q;
    if (q.empty()) {  // empty query: back to the original list (the same object, not a copy)
        nodes_ = groupNodes_;
        return true;
    }
    auto result = filterNodes(q, *groupNodes_, [](BeatmapSetNode* n) -> const BeatmapSet& { return n->getBeatmapSet(); });
    nodes_ = std::make_shared<NodeList>(std::move(*result));  // no cancel hook, so never nullopt
    return true;
}

void BeatmapSetList::setRandomSource(std::function<double()> source) { random_ = std::move(source); }

}  // namespace opsu

// ---------------------------------------------------------------------------------------------------------------- batch 35: deletion
namespace opsu {
namespace {

// java.io.File path helpers (Unix separators): the path is normalised like `new File(path)` (duplicate and trailing '/' removed).
std::string normalizePath(const std::string& p) {
    std::string out;
    for (char c : p) {
        if (c == '/' && !out.empty() && out.back() == '/') continue;
        out.push_back(c);
    }
    if (out.size() > 1 && out.back() == '/') out.pop_back();
    return out;
}
/** File.getName(): the part after the last '/' ("" for the root). */
std::string fileName(const std::string& path) {
    std::string p = normalizePath(path);
    std::size_t i = p.rfind('/');
    return i == std::string::npos ? p : p.substr(i + 1);
}
/** File.getParent(): nullopt (Java null) when there is none. */
std::optional<std::string> parentPath(const std::string& path) {
    std::string p = normalizePath(path);
    std::size_t i = p.rfind('/');
    if (i == std::string::npos) return std::nullopt;
    if (i == 0) return p.size() > 1 ? std::optional<std::string>("/") : std::nullopt;  // "/x" -> "/", "/" -> null
    return p.substr(0, i);
}
/** `file.getParentFile().getName()`: Java throws NullPointerException when there is no parent. */
std::string parentDirName(const std::string& path) {
    std::optional<std::string> parent = parentPath(path);
    if (!parent) throw std::logic_error("NullPointerException: file has no parent: " + path);
    return fileName(*parent);
}
template <class T>
T* nonNull(T* p, const char* what) {
    if (!p) throw std::logic_error(std::string("NullPointerException: ") + what);
    return p;
}

}  // namespace

bool BeatmapSetList::deleteSongGroup(BeatmapSetNode* node) {
    if (node == nullptr) return false;

    // re-link base nodes
    int index = node->index;
    BeatmapSetNode *ePrev = getBaseNode(index - 1), *eCur = getBaseNode(index), *eNext = getBaseNode(index + 1);
    if (ePrev != nullptr) {
        if (ePrev->index == expandedIndex_) nonNull(expandedEnd_, "expandedEndNode")->next = eNext;
        else if (eNext != nullptr && eNext->index == expandedIndex_) ePrev->next = expandedStart_;
        else ePrev->next = eNext;
    }
    if (eNext != nullptr) {
        if (eNext->index == expandedIndex_) nonNull(expandedStart_, "expandedStartNode")->prev = ePrev;
        else if (ePrev != nullptr && ePrev->index == expandedIndex_) eNext->prev = expandedEnd_;
        else eNext->prev = ePrev;
    }

    // remove all node references
    BeatmapSet& beatmapSet = node->getBeatmapSet();
    Beatmap beatmap = beatmapSet.get(0);  // a copy: Java keeps the object, the set may change below
    if (index < 0 || index >= size()) throw std::out_of_range("IndexOutOfBoundsException: nodes.remove(" + std::to_string(index) + ")");
    nodes_->erase(nodes_->begin() + index);
    auto removeFirst = [](NodeList& list, BeatmapSetNode* n) {  // ArrayList.remove(Object): first equal element, if any
        auto it = std::find(list.begin(), list.end(), n);
        if (it != list.end()) list.erase(it);
    };
    removeFirst(*parsedNodes_, eCur);
    removeFirst(*groupNodes_, eCur);
    mapCount_ -= static_cast<int>(beatmapSet.size());
    if (beatmap.beatmapSetID > 0) msidDb_.erase(beatmap.beatmapSetID);
    for (const Beatmap& bm : beatmapSet) {
        if (!bm.md5Hash.empty()) beatmapHashDb_.erase(bm.md5Hash);
    }

    // reset indices
    for (int i = index, n = size(); i < n; i++) (*nodes_)[static_cast<std::size_t>(i)]->index = i;
    if (index == expandedIndex_) {
        expandedIndex_ = -1;
        expandedStart_ = expandedEnd_ = nullptr;
    } else if (expandedIndex_ > index) {
        expandedIndex_--;
        BeatmapSetNode* expandedNode = nonNull(expandedStart_, "expandedStartNode");
        for (int i = 0, n = static_cast<int>(expandedNode->getBeatmapSet().size()); i < n && expandedNode != nullptr; i++, expandedNode = expandedNode->next)
            expandedNode->index = expandedIndex_;
    }

    // stop playing the track
    std::optional<std::string> dir = parentPath(beatmap.getFile());
    if (hooks_.trackActive && hooks_.trackActive()) {
        std::string audioFile = hooks_.currentAudioFilename ? hooks_.currentAudioFilename() : std::string();
        if (!audioFile.empty() && audioFile == beatmap.audioFilename) {
            if (hooks_.resetTrack) hooks_.resetTrack();
        }
    }

    // remove entry from cache
    if (!dir) throw std::logic_error("NullPointerException: beatmap file has no parent: " + beatmap.getFile());
    if (hooks_.dbDeleteDir) hooks_.dbDeleteDir(fileName(*dir));

    // delete the associated directory
    bool ws = hooks_.watchServiceActive && hooks_.watchServiceActive();
    if (ws && hooks_.pauseWatchService) hooks_.pauseWatchService();
    try {
        if (hooks_.deleteToTrash) hooks_.deleteToTrash(*dir);
    } catch (const java::IOException& e) {
        Download::reportError("Could not delete song group.", &e, true);
    }
    if (ws && hooks_.resumeWatchService) hooks_.resumeWatchService();

    return true;
}

bool BeatmapSetList::deleteSong(BeatmapSetNode* node) {
    if (node == nullptr || node->beatmapIndex == -1 || node->index != expandedIndex_) return false;

    // last song in group?
    int size = static_cast<int>(node->getBeatmapSet().size());
    if (size == 1) return deleteSongGroup(node);

    // reset indices
    BeatmapSetNode* expandedNode = node->next;
    for (int i = node->beatmapIndex + 1; i < size && expandedNode != nullptr && expandedNode->index == node->index; i++, expandedNode = expandedNode->next)
        expandedNode->beatmapIndex--;

    // remove song reference
    if (node->beatmapIndex < 0) throw std::out_of_range("IndexOutOfBoundsException: remove(" + std::to_string(node->beatmapIndex) + ")");
    Beatmap beatmap = node->getBeatmapSet().remove(static_cast<std::size_t>(node->beatmapIndex));  // throws std::out_of_range if too large
    mapCount_--;
    if (!beatmap.md5Hash.empty()) beatmapHashDb_.erase(beatmap.md5Hash);

    // re-link nodes
    if (node->prev != nullptr) node->prev->next = node->next;
    if (node->next != nullptr) node->next->prev = node->prev;

    // remove entry from cache
    const std::string file = beatmap.getFile();
    if (hooks_.dbDeleteFile) hooks_.dbDeleteFile(parentDirName(file), fileName(file));

    // delete the associated file
    bool ws = hooks_.watchServiceActive && hooks_.watchServiceActive();
    if (ws && hooks_.pauseWatchService) hooks_.pauseWatchService();
    try {
        if (hooks_.deleteToTrash) hooks_.deleteToTrash(normalizePath(file));
    } catch (const java::IOException& e) {
        Download::reportError("Could not delete song.", &e, true);
    }
    if (ws && hooks_.resumeWatchService) hooks_.resumeWatchService();

    return true;
}

}  // namespace opsu
