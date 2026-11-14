#include "opsu/beatmap/BeatmapGroup.h"
#include "opsu/beatmap/BeatmapSetNode.h"
#include <stdexcept>
#include "opsu/util/JavaMath.h"

namespace opsu {

namespace {
BeatmapGroup currentValue = BeatmapGroup::ALL;

struct Tab {
    bool laidOut = false;
    float x = 0, y = 0;
    int width = 0;
};
Tab tabs[BeatmapGroup_COUNT];

const Tab& tab(BeatmapGroup g) {
    const Tab& t = tabs[static_cast<int>(g)];
    if (!t.laidOut) throw std::logic_error("BeatmapGroup tab used before init()");
    return t;
}

// java.util.PriorityQueue<Integer> over indices, with the comparator Long2.compare(lastPlayed(v), lastPlayed(w)).
// siftUp / siftDown are the JDK's (SiftUp / SiftDown in PriorityQueue.java), so iteration order matches.
struct Heap {
    const std::vector<const BeatmapSetBeatmaps*>& sets;
    std::vector<std::size_t> q;

    int cmp(std::size_t a, std::size_t b) const { return java::long2Compare(lastPlayed(*sets[a]), lastPlayed(*sets[b])); }

    void add(std::size_t x) {
        std::size_t k = q.size();
        q.push_back(x);
        if (k == 0) return;
        while (k > 0) {
            std::size_t parent = (k - 1) >> 1;
            std::size_t e = q[parent];
            if (cmp(x, e) >= 0) break;
            q[k] = e;
            k = parent;
        }
        q[k] = x;
    }

    void poll() {
        std::size_t n = q.size() - 1;
        std::size_t x = q[n];
        q.pop_back();
        if (n == 0) return;
        std::size_t k = 0, half = n >> 1;
        while (k < half) {
            std::size_t child = 2 * k + 1;
            std::size_t c = q[child];
            std::size_t right = child + 1;
            if (right < n && cmp(c, q[right]) > 0) c = q[child = right];
            if (cmp(x, c) <= 0) break;
            q[k] = c;
            k = child;
        }
        q[k] = x;
    }
};
}  // namespace

const char* name(BeatmapGroup g) {
    switch (g) {
        case BeatmapGroup::ALL: return "ALL";
        case BeatmapGroup::RECENT: return "RECENT";
        case BeatmapGroup::FAVORITE: return "FAVORITE";
    }
    return "";
}
const char* getName(BeatmapGroup g) {
    switch (g) {
        case BeatmapGroup::ALL: return "All Songs";
        case BeatmapGroup::RECENT: return "Last Played";
        case BeatmapGroup::FAVORITE: return "Favorites";
    }
    return "";
}
int getId(BeatmapGroup g) { return static_cast<int>(g); }
const char* getEmptyMessage(BeatmapGroup g) {
    switch (g) {
        case BeatmapGroup::ALL: return nullptr;
        case BeatmapGroup::RECENT: return "Your recently played beatmaps will appear in this list!";
        case BeatmapGroup::FAVORITE: return "Right-click a beatmap to add it to your Favorites!";
    }
    return nullptr;
}

BeatmapGroup currentGroup() { return currentValue; }
void setGroup(BeatmapGroup g) { currentValue = g; }

int64_t lastPlayed(const BeatmapSetBeatmaps& set) {
    int64_t max = 0;
    for (const Beatmap& b : set)
        if (b.lastPlayed > max) max = b.lastPlayed;
    return max;
}

bool isFavorite(const BeatmapSetBeatmaps& set) {
    for (const Beatmap& b : set)
        if (b.favorite) return true;
    return false;
}

std::vector<std::size_t> filter(BeatmapGroup g, const std::vector<const BeatmapSetBeatmaps*>& sets) {
    std::vector<std::size_t> out;
    switch (g) {
        case BeatmapGroup::ALL:
            for (std::size_t i = 0; i < sets.size(); i++) out.push_back(i);
            break;
        case BeatmapGroup::RECENT: {
            Heap pq{sets, {}};
            for (std::size_t i = 0; i < sets.size(); i++) {
                int64_t timestamp = lastPlayed(*sets[i]);
                if (timestamp == 0) continue;  // skip unplayed beatmaps
                if (pq.q.size() < static_cast<std::size_t>(RECENT_K) || timestamp > lastPlayed(*sets[pq.q[0]])) {
                    if (pq.q.size() == static_cast<std::size_t>(RECENT_K)) pq.poll();
                    pq.add(i);
                }
            }
            out = pq.q;  // iteration order of a PriorityQueue is its array order
            break;
        }
        case BeatmapGroup::FAVORITE:
            for (std::size_t i = 0; i < sets.size(); i++)
                if (isFavorite(*sets[i])) out.push_back(i);
            break;
    }
    return out;
}

void init(BeatmapGroup g, int containerWidth, float bottomY, int tabWidth, int tabHeight) {
    const int SIZE = static_cast<int>(BeatmapGroup_COUNT);
    float buttonX = containerWidth / 2.f;
    float tabOffset = (containerWidth - buttonX - tabWidth) / (SIZE - 1);
    if (tabOffset > tabWidth) {  // prevent tabs from being spaced out
        tabOffset = tabWidth * 1.04f;  // small gap between tabs
        buttonX = (containerWidth * 0.99f) - (tabOffset * (SIZE - 1) + tabWidth);
    }
    Tab& t = tabs[static_cast<int>(g)];
    t.x = (buttonX + (tabWidth / 2.f)) + (getId(g) * tabOffset);
    t.y = bottomY - (tabHeight / 2.f);
    t.width = tabWidth;
    t.laidOut = true;
}

float getTabCenterX(BeatmapGroup g) { return tab(g).x; }
float getTabCenterY(BeatmapGroup g) { return tab(g).y; }
float getTabLeft(BeatmapGroup g) { const Tab& t = tab(g); return t.x - t.width / 2.f; }

std::vector<BeatmapSetNode*> filter(BeatmapGroup g, const std::vector<BeatmapSetNode*>& list) {
    return filter(g, list, [](BeatmapSetNode* n) -> const BeatmapSetBeatmaps& { return n->getBeatmapSet().beatmaps(); });
}

}  // namespace opsu
