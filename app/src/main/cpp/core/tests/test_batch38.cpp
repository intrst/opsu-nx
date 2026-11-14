// Tests for batch 38: BeatmapDB single-row updates over a fake store, and the BeatmapSetNode-typed BeatmapGroup::filter.
#include <cstdio>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapGroup.h"
#include "opsu/beatmap/BeatmapSetNode.h"
#include "opsu/db/BeatmapDB.h"
#include "opsu/downloads/Download.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

struct Store : BeatmapDBStore {
    std::vector<std::string> log; bool fail = false;
    int deleteMap(const std::string&, const std::string&) override { return 0; }
    int deleteGroup(const std::string&) override { return 0; }
    void updateCacheSize(const std::string&) override {}
    void setStars(double s, const std::string& d, const std::string& f) override { log.push_back("stars:" + std::to_string(s) + "|" + d + "|" + f); if (fail) throw SQLException("x"); }
    void updatePlayStats(int c, int64_t t, const std::string& d, const std::string& f) override { log.push_back("play:" + std::to_string(c) + "," + std::to_string(t) + "|" + d + "|" + f); if (fail) throw SQLException("x"); }
    void setFavorite(bool b, const std::string& d, const std::string& f) override { log.push_back(std::string("fav:") + (b ? "1" : "0") + "|" + d + "|" + f); if (fail) throw SQLException("x"); }
    void setLocalOffset(int o, const std::string& d, const std::string& f) override { log.push_back("off:" + std::to_string(o) + "|" + d + "|" + f); if (fail) throw SQLException("x"); }
};
struct Reports {
    std::vector<std::string> msgs;
    Reports() { Download::setErrorHandler([this](const std::string& m, const std::exception*, bool) { msgs.push_back(m); }); }
    ~Reports() { Download::setErrorHandler({}); }
};

static Beatmap mk(const std::string& path) {
    Beatmap b(path); b.artist = "Art"; b.title = "Ti"; b.version = "Ver"; return b;
}

int main() {
    {   // field mapping and dir / file split
        Reports rp; Store st; BeatmapDB db(&st);
        Beatmap b = mk("Songs/dirA/A1.osu"); b.starRating = 4.5; b.playCount = 7; b.lastPlayed = 1234567890123LL; b.favorite = true; b.localMusicOffset = -15;
        db.setStars(b); db.updatePlayStatistics(b); db.updateFavoriteStatus(b); db.updateLocalOffset(b);
        CHECK(st.log.size() == 4);
        CHECK(st.log[0] == "stars:4.500000|dirA|A1.osu");
        CHECK(st.log[1] == "play:7,1234567890123|dirA|A1.osu");
        CHECK(st.log[2] == "fav:1|dirA|A1.osu");
        CHECK(st.log[3] == "off:-15|dirA|A1.osu");
        CHECK(rp.msgs.empty());
        st.log.clear(); Beatmap c = mk("/x//d2/f.osu/"); db.updateFavoriteStatus(c);   // File normalises '//' and the trailing '/'
        CHECK(st.log.size() == 1 && st.log[0] == "fav:0|d2|f.osu");
        st.log.clear(); Beatmap r = mk("/f.osu"); db.updateLocalOffset(r);               // parent is "/", whose name is ""
        CHECK(st.log.size() == 1 && st.log[0] == "off:0||f.osu");
    }
    {   // no connection: nothing happens
        Reports rp; BeatmapDB db; Beatmap b = mk("a/b.osu");
        db.setStars(b); db.updatePlayStatistics(b); db.updateFavoriteStatus(b); db.updateLocalOffset(b);
        CHECK(rp.msgs.empty());
        Beatmap bare = mk("bare.osu"); db.setStars(bare);   // no parent, but no connection: returns before touching it
        CHECK(rp.msgs.empty());
    }
    {   // SQL failures: Java's messages (stars use %.4f HALF_UP)
        Reports rp; Store st; st.fail = true; BeatmapDB db(&st);
        Beatmap b = mk("s/d/f.osu"); b.starRating = 1.00005;
        db.setStars(b); db.updatePlayStatistics(b); db.updateFavoriteStatus(b); db.updateLocalOffset(b);
        CHECK(rp.msgs.size() == 4);
        CHECK(rp.msgs[0] == "Failed to save star rating '1.0001' for beatmap '" + b.toString(false) + "' in database.");
        CHECK(rp.msgs[1] == "Failed to update play statistics for beatmap '" + b.toString(false) + "' in database.");
        CHECK(rp.msgs[2] == "Failed to update favorite status for beatmap '" + b.toString(false) + "' in database.");
        CHECK(rp.msgs[3] == "Failed to update local music offset for beatmap '" + b.toString(false) + "' in database.");
        rp.msgs.clear(); b.artist = ""; db.updateFavoriteStatus(b, true);
        CHECK(rp.msgs.size() == 1 && rp.msgs[0].find(b.toString(true)) != std::string::npos);
    }
    {   // a bare file name has no parent: NullPointerException in Java, not an SQLException -> propagates, nothing stored
        Store st; BeatmapDB db(&st); Beatmap bare = mk("bare.osu"); bool threw = false;
        try { db.setStars(bare); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw && st.log.empty());
    }
    {   // BeatmapGroup::filter over real nodes
        Beatmap::ModParams mp = Beatmap::ModParams::current(1.f, 0.f);
        auto mkNode = [&](int64_t played, bool fav) {
            Beatmap b = mk("d/x.osu"); b.lastPlayed = played; b.favorite = fav;
            return new BeatmapSetNode(BeatmapSet(std::vector<Beatmap>{b}, mp));
        };
        std::vector<BeatmapSetNode*> nodes{mkNode(0, false), mkNode(50, true), mkNode(10, false), mkNode(30, true)};
        CHECK(filter(BeatmapGroup::ALL, nodes) == nodes);
        auto fav = filter(BeatmapGroup::FAVORITE, nodes);
        CHECK(fav.size() == 2 && fav[0] == nodes[1] && fav[1] == nodes[3]);
        auto rec = filter(BeatmapGroup::RECENT, nodes);                 // unplayed skipped; heap order of 50, 10, 30
        CHECK(rec.size() == 3 && rec[0] == nodes[2] && rec[1] == nodes[1] && rec[2] == nodes[3]);
        std::vector<BeatmapSetNode*> many;
        for (int i = 1; i <= 25; i++) many.push_back(mkNode(i * 10, false));
        auto top = filter(BeatmapGroup::RECENT, many);
        CHECK(top.size() == 20);
        for (auto* n : top) CHECK(n->getBeatmapSet().beatmaps()[0].lastPlayed > 50);   // the 5 oldest were dropped
        for (auto* n : nodes) delete n;
        for (auto* n : many) delete n;
    }
    std::printf(failures ? "batch38: %d FAILED\n" : "batch38: all passed\n", failures);
    return failures ? 1 : 0;
}
