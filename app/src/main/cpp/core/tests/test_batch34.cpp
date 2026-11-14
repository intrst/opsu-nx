// Tests for batch 34 (BeatmapSetList: MD5 hash table + getBeatmapFromHash). Behavioural; no JDK here.
#include <cstdio>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapSetList.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);
// beatmaps with the given (hash, star) pairs; the file name is "<title><i>.osu"
static std::vector<Beatmap> mk(const std::string& title, std::vector<std::pair<std::string, float>> specs) {
    std::vector<Beatmap> v; int i = 0;
    for (auto& sp : specs) {
        Beatmap b("/x/" + title + std::to_string(i) + ".osu");
        b.title = title; b.version = "v" + std::to_string(i); b.md5Hash = sp.first; b.starRating = sp.second; b.beatmapSetID = 1;
        v.push_back(b); i++;
    }
    return v;
}

int main() {
    BeatmapSetList l;
    CHECK(l.getBeatmapFromHash("aaa") == nullptr && l.getBeatmapFromHash("") == nullptr);  // empty list

    BeatmapSetNode* n1 = l.addSongGroup(mk("One", {{"h1", 1}, {"h2", 2}, {"", 3}}), NOMOD);
    BeatmapSetNode* n2 = l.addSongGroup(mk("Two", {{"h3", 1}}), NOMOD);
    const Beatmap* b = l.getBeatmapFromHash("h1");
    CHECK(b && b->title == "One" && b->version == "v0" && b == &n1->getBeatmapSet().get(0));  // a pointer INTO the set, not a copy
    CHECK(l.getBeatmapFromHash("h2") == &n1->getBeatmapSet().get(1));
    CHECK(l.getBeatmapFromHash("h3") == &n2->getBeatmapSet().get(0));
    CHECK(l.getBeatmapFromHash("nope") == nullptr);
    CHECK(l.getBeatmapFromHash("") == nullptr);  // the third beatmap has no hash: nothing stored under "" (Java: get(null))
    CHECK(l.getBeatmapFromHash("H1") == nullptr);  // case sensitive

    // a duplicate hash: the later beatmap wins (HashMap.put overwrites), across sets and inside one set (after the sort)
    BeatmapSetNode* n3 = l.addSongGroup(mk("Three", {{"h1", 1}}), NOMOD);
    CHECK(l.getBeatmapFromHash("h1") == &n3->getBeatmapSet().get(0));
    BeatmapSetNode* n4 = l.addSongGroup(mk("Four", {{"dup", 5}, {"dup", 2}}), NOMOD);  // sorted by star: v1 (2), then v0 (5)
    const Beatmap* d = l.getBeatmapFromHash("dup");
    CHECK(d && d->version == "v0" && d == &n4->getBeatmapSet().get(1));

    // sortByDifficulty moves the Beatmap values inside the set: the lookup must still reach the right beatmap
    BeatmapSetNode* n5 = l.addSongGroup(mk("Five", {{"s1", 1}, {"s2", 2}, {"", 3}}), NOMOD);
    BeatmapSet& s5 = n5->getBeatmapSet();
    CHECK(s5.get(0).md5Hash == "s1" && s5.get(1).md5Hash == "s2" && s5.get(2).md5Hash.empty());
    CHECK(l.getBeatmapFromHash("s1") == &s5.get(0));
    s5.get(0).starRating = 9;  // s1 becomes the hardest
    CHECK(s5.sortByDifficulty(NOMOD));
    CHECK(s5.get(0).md5Hash == "s2" && s5.get(1).md5Hash.empty() && s5.get(2).md5Hash == "s1");
    CHECK(l.getBeatmapFromHash("s1") == &s5.get(2) && l.getBeatmapFromHash("s1")->version == "v0");
    CHECK(l.getBeatmapFromHash("s2") == &s5.get(0) && l.getBeatmapFromHash("s2")->version == "v1");

    // counts untouched by the table
    CHECK(l.getMapCount() == 3 + 1 + 1 + 2 + 3 && l.getMapSetCount() == 5);

    // an entry whose beatmap left its set is a miss, not a crash
    n2->getBeatmapSet().remove(0);
    CHECK(l.getBeatmapFromHash("h3") == nullptr);

    if (failures == 0) std::printf("batch34 (BeatmapSetList hash table): all tests passed\n");
    return failures ? 1 : 0;
}
