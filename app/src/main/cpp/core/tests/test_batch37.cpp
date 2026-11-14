// Tests for batch 37: the BeatmapDB delete slice (cacheSize bookkeeping, error reporting) over a fake store, plus an integration test through BeatmapSetList's DB hooks.
#include <climits>
#include <cstdio>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapSetList.h"
#include "opsu/db/BeatmapDB.h"
#include "opsu/downloads/Download.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

struct FakeStore : BeatmapDBStore {
    std::vector<std::string> log;
    int rows = 1;                       // rows the next delete reports
    bool failDelete = false, failSize = false, failOther = false;
    int deleteMap(const std::string& d, const std::string& f) override {
        log.push_back("map:" + d + "|" + f);
        if (failOther) throw std::logic_error("not an SQL problem");
        if (failDelete) throw SQLException("locked");
        return rows;
    }
    int deleteGroup(const std::string& d) override {
        log.push_back("group:" + d);
        if (failDelete) throw SQLException("locked");
        return rows;
    }
    void setStars(double, const std::string&, const std::string&) override {}
    void updatePlayStats(int, int64_t, const std::string&, const std::string&) override {}
    void setFavorite(bool, const std::string&, const std::string&) override {}
    void setLocalOffset(int, const std::string&, const std::string&) override {}
    void updateCacheSize(const std::string& v) override {
        log.push_back("size:" + v);
        if (failSize) throw SQLException("disk full");
    }
};

struct Reports {
    std::vector<std::string> msgs, whats; std::vector<bool> flags;
    Reports() {
        Download::setErrorHandler([this](const std::string& m, const std::exception* e, bool r) { msgs.push_back(m); whats.push_back(e ? e->what() : "<null>"); flags.push_back(r); });
    }
    ~Reports() { Download::setErrorHandler({}); }
};

int main() {
    {   // no connection: nothing happens, no calls, no reports
        Reports rp; BeatmapDB db;
        CHECK(!db.hasConnection() && db.cacheSize() == -1);
        db.deleteMap("d", "f"); db.deleteGroup("d");
        CHECK(db.cacheSize() == -1 && rp.msgs.empty());
        FakeStore st; db.setStore(&st); CHECK(db.hasConnection());
        db.setStore(nullptr); db.deleteGroup("d"); CHECK(st.log.empty());
    }
    {   // a delete reduces cacheSize by the row count and writes Math.max(cacheSize, 0)
        Reports rp; FakeStore st; BeatmapDB db(&st); db.setCacheSize(10);
        st.rows = 3; db.deleteMap("dirA", "A.osu");
        CHECK(db.cacheSize() == 7 && st.log == (std::vector<std::string>{"map:dirA|A.osu", "size:7"}));
        st.log.clear(); st.rows = 4; db.deleteGroup("dirB");
        CHECK(db.cacheSize() == 3 && st.log == (std::vector<std::string>{"group:dirB", "size:3"}));
        // goes below zero: the field keeps the negative value, the stored string is clamped
        st.log.clear(); st.rows = 10; db.deleteGroup("dirC");
        CHECK(db.cacheSize() == -7 && st.log == (std::vector<std::string>{"group:dirC", "size:0"}));
        st.log.clear(); st.rows = 0; db.deleteMap("d", "f");                    // 0 rows: still writes the size
        CHECK(db.cacheSize() == -7 && st.log.back() == "size:0" && rp.msgs.empty());
    }
    {   // default cacheSize -1 (not loaded yet)
        FakeStore st; BeatmapDB db(&st); st.rows = 1; db.deleteGroup("x");
        CHECK(db.cacheSize() == -2 && st.log.back() == "size:0");
    }
    {   // int wrap-around like Java
        FakeStore st; BeatmapDB db(&st); db.setCacheSize(INT_MIN); st.rows = 1; db.deleteGroup("x");
        CHECK(db.cacheSize() == INT_MAX && st.log.back() == "size:2147483647");
    }
    {   // a failing delete: reported with the delete message, cacheSize unchanged, no size write
        Reports rp; FakeStore st; BeatmapDB db(&st); db.setCacheSize(5); st.failDelete = true;
        db.deleteMap("d", "f");
        CHECK(db.cacheSize() == 5 && st.log == (std::vector<std::string>{"map:d|f"}));
        CHECK(rp.msgs.size() == 1 && rp.msgs[0] == "Failed to delete beatmap entry from database." && rp.whats[0] == "locked" && rp.flags[0]);
        db.deleteGroup("d");
        CHECK(rp.msgs.size() == 2 && rp.msgs[1] == "Failed to delete beatmap group entry from database." && db.cacheSize() == 5);
    }
    {   // a failing size update: its own message only (the decrement already happened)
        Reports rp; FakeStore st; BeatmapDB db(&st); db.setCacheSize(5); st.failSize = true; st.rows = 2;
        db.deleteGroup("d");
        CHECK(db.cacheSize() == 3 && rp.msgs.size() == 1 && rp.msgs[0] == "Could not update beatmap cache size." && rp.whats[0] == "disk full" && rp.flags[0]);
        db.deleteMap("d", "f");
        CHECK(db.cacheSize() == 1 && rp.msgs.size() == 2 && rp.msgs[1] == "Could not update beatmap cache size.");
    }
    {   // only SQLException is caught (Java: catch SQLException)
        Reports rp; FakeStore st; BeatmapDB db(&st); db.setCacheSize(5); st.failOther = true;
        bool threw = false; try { db.deleteMap("d", "f"); } catch (const std::logic_error&) { threw = true; }
        CHECK(threw && db.cacheSize() == 5 && rp.msgs.empty());
    }
    {   // integration: BeatmapSetList::deleteSong / deleteSongGroup call the DB hooks with the DIRECTORY NAME (not path) and file name
        FakeStore st; BeatmapDB db(&st); db.setCacheSize(10); st.rows = 1;
        BeatmapSetList l;
        BeatmapSetList::DeleteHooks h;
        h.dbDeleteDir = [&](const std::string& d) { db.deleteGroup(d); };
        h.dbDeleteFile = [&](const std::string& d, const std::string& f) { db.deleteMap(d, f); };
        l.setDeleteHooks(h);
        const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);
        std::vector<Beatmap> v;
        for (int j = 0; j < 2; j++) {
            Beatmap b("/songs/dirA/A" + std::to_string(j) + ".osu");
            b.title = "A"; b.md5Hash = "hA" + std::to_string(j); b.beatmapSetID = 100; b.starRating = static_cast<float>(j + 1);
            v.push_back(b);
        }
        l.addSongGroup(v, NOMOD);
        l.init();
        BeatmapSetNode* n = l.expand(0);
        BeatmapSetNode* second = n;
        for (int k = 0; k < 6 && second && second->beatmapIndex != 1; k++) second = second->next;
        CHECK(second && second->beatmapIndex == 1);
        CHECK(l.deleteSong(second));
        CHECK(st.log == (std::vector<std::string>{"map:dirA|A1.osu", "size:9"}));
        st.log.clear();
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        CHECK(st.log == (std::vector<std::string>{"group:dirA", "size:8"}) && db.cacheSize() == 8);
    }
    if (failures == 0) std::printf("batch37: all tests passed\n");
    return failures ? 1 : 0;
}
