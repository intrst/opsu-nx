// Tests for batch 29 (BeatmapSet). Behavioural (no JDK in the build environment, so not a golden test).
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include "opsu/GameMod.h"
#include "opsu/beatmap/BeatmapSet.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { std::printf("FAIL %s:%d  %s != %s\n  got: %s\n", __FILE__, __LINE__, #a, #b, std::string(_a).c_str()); failures++; } } while (0)

static Beatmap mk(const std::string& version, double stars, float od = 5.f, int circles = 10) {
    Beatmap b("/x/" + version + ".osu");
    b.version = version; b.starRating = stars; b.overallDifficulty = od; b.hitObjectCircle = circles;
    b.title = "Title"; b.artist = "Artist"; b.creator = "Mapper";
    return b;
}
static std::vector<std::string> versions(const BeatmapSet& s) {
    std::vector<std::string> v; for (const Beatmap& b : s) v.push_back(b.version); return v;
}
static const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);

int main() {
    GameMod::resetState();

    // ---- ordering ----
    {
        BeatmapSet s({mk("Insane", 4.5), mk("Easy", 1.2), mk("Unrated", -1), mk("Hard", 3.3)}, NOMOD);
        CHECK(versions(s) == (std::vector<std::string>{"Unrated", "Easy", "Hard", "Insane"}));
        CHECK(!s.sortByDifficulty(NOMOD));  // already sorted: unchanged
        s.get(0).starRating = 9;            // "Unrated" now the hardest
        CHECK(s.sortByDifficulty(NOMOD));
        CHECK(versions(s) == (std::vector<std::string>{"Easy", "Hard", "Insane", "Unrated"}));
    }
    {   // ties: OD, then object count, then version ignoring case; fully equal keeps input order (stable)
        BeatmapSet s({mk("b", 2, 5, 10), mk("a", 2, 5, 10), mk("c", 2, 4, 99), mk("d", 2, 5, 3)}, NOMOD);
        CHECK(versions(s) == (std::vector<std::string>{"c", "d", "a", "b"}));
        BeatmapSet t({mk("X", 2), mk("x", 2), mk("Y", 2)}, NOMOD);
        CHECK(versions(t) == (std::vector<std::string>{"X", "x", "Y"}));  // X == x ignoring case: stable
    }
    {   // mod stars are used when they belong to the current mods
        Beatmap::ModParams dt = Beatmap::ModParams::of(0.f, 1.5f, 1.f, 0.f);
        Beatmap a = mk("a", 1), b = mk("b", 2);
        a.setModStars(dt, 5.0, dt);
        BeatmapSet s({b, a}, dt);
        CHECK(versions(s) == (std::vector<std::string>{"b", "a"}));  // a is 5.0 under DT
        BeatmapSet u({b, a}, NOMOD);
        CHECK(versions(u) == (std::vector<std::string>{"a", "b"}));
    }
    {   // stability on a larger input (libstdc++ std::sort is insertion sort below 17 elements, so 90 are needed)
        std::vector<Beatmap> in;
        for (int i = 0; i < 90; i++) { Beatmap b = mk("same", (i % 3)); b.beatmapID = i; in.push_back(b); }
        BeatmapSet s(in, NOMOD);
        bool ok = true; int prevStars = -2, prevId = -1;
        for (const Beatmap& b : s) {
            int st = static_cast<int>(b.starRating);
            if (st == prevStars && b.beatmapID < prevId) ok = false;
            if (st < prevStars) ok = false;
            prevStars = st; prevId = b.beatmapID;
        }
        CHECK(ok && s.size() == 90);
    }
    CHECK(BeatmapSet({}, NOMOD).size() == 0);

    // ---- access ----
    {
        BeatmapSet s({mk("a", 1), mk("b", 2), mk("c", 3)}, NOMOD);
        CHECK(s.size() == 3 && s.get(1).version == "b");
        Beatmap r = s.remove(1);
        CHECK(r.version == "b" && s.size() == 2 && s.get(1).version == "c");
        bool threw = false; try { s.get(2); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);
        threw = false; try { s.remove(5); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);
        threw = false; try { BeatmapSet({}, NOMOD).toString(false); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);
        threw = false; try { BeatmapSet({}, NOMOD).matches("x"); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);
        threw = false; try { s.getInfo(2, false, NOMOD); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);
        CHECK(!BeatmapSet({}, NOMOD).matches("ar", "=", 1.f));
    }

    // ---- toString / getInfo ----
    {
        Beatmap b = mk("Insane", 4.567);
        b.titleUnicode = "\xE3\x82\xBF"; b.artistUnicode = "\xE3\x82\xA2"; b.creator = "Me";
        b.endTime = 185000; b.bpmMin = 120; b.bpmMax = 120;
        b.hitObjectCircle = 100; b.hitObjectSlider = 20; b.hitObjectSpinner = 2;
        b.circleSize = 4.f; b.HPDrainRate = 5.55f; b.approachRate = 9.f; b.overallDifficulty = 7.25f;
        BeatmapSet s({b}, NOMOD);
        CHECK_EQ(s.toString(false), std::string("Artist - Title"));
        CHECK_EQ(s.toString(true), std::string("\xE3\x82\xA2 - \xE3\x82\xBF"));
        auto i = s.getInfo(0, false, NOMOD);
        CHECK(i.size() == 5);
        CHECK_EQ(i[0], std::string("Artist - Title [Insane]"));
        CHECK_EQ(i[1], std::string("Mapped by Me"));
        CHECK_EQ(i[2], std::string("Length: 3:05  BPM: 120  Objects: 122"));
        CHECK_EQ(i[3], std::string("Circles: 100  Sliders: 20  Spinners: 2"));
        // 5.55f is 5.5500002 as a float -> "5.6"; 7.25 is exact, HALF_EVEN -> "7.2"
        CHECK_EQ(i[4], std::string("CS:4 HP:5.6 AR:9 OD:7.2 Stars:4.57"));
        // BPM range, no stars, negative end time
        Beatmap c = mk("x", -1); c.bpmMin = 100; c.bpmMax = 200; c.endTime = 59999; c.approachRate = 0.f; c.circleSize = 0.f;
        auto j = BeatmapSet({c}, NOMOD).getInfo(0, false, NOMOD);
        CHECK_EQ(j[2], std::string("Length: 0:59  BPM: 100-200  Objects: 10"));
        CHECK_EQ(j[4], std::string("CS:0 HP:5 AR:0 OD:5"));  // 0 prints "0", no stars suffix
        c.bpmMax = 0; c.endTime = -1;
        j = BeatmapSet({c}, NOMOD).getInfo(0, false, NOMOD);
        CHECK_EQ(j[2], std::string("Length: 0:00  BPM: --  Objects: 10"));
        c.endTime = 3600000 * 2 + 61000;
        j = BeatmapSet({c}, NOMOD).getInfo(0, false, NOMOD);
        CHECK(j[2].rfind("Length: 121:01", 0) == 0);  // minutes are not wrapped at 60

        // mods: DT speeds up, HR multiplies difficulty (capped at 10)
        GameMod::DOUBLE_TIME.toggle(false);
        auto k = s.getInfo(0, false, NOMOD);
        CHECK_EQ(k[2], std::string("Length: 2:03  BPM: 180  Objects: 122"));  // 185000/1.5 = 123333 ms; 120*1.5
        GameMod::resetState();
        GameMod::HARD_ROCK.toggle(false);
        auto m = s.getInfo(0, false, NOMOD);
        CHECK_EQ(m[4], std::string("CS:5.6 HP:7.8 AR:10 OD:10 Stars:4.57"));  // x1.4 (float), min 10
        GameMod::resetState();
        // the cap: 8 * 1.4 = 11.2 -> 10 for all four values
        Beatmap d = mk("d", 1); d.circleSize = d.HPDrainRate = d.approachRate = d.overallDifficulty = 8.f;
        GameMod::HARD_ROCK.toggle(false);
        CHECK_EQ(BeatmapSet({d}, NOMOD).getInfo(0, false, NOMOD)[4], std::string("CS:10 HP:10 AR:10 OD:10 Stars:1.00"));
        GameMod::resetState();
    }

    // ---- matches(query) ----
    {
        Beatmap a = mk("Normal", 1), b = mk("Hell Insane", 5);
        a.titleUnicode = "Tt"; a.artistUnicode = "Aa"; a.source = "Game Src"; a.tags = "foo bar"; a.beatmapSetID = 123456;
        b.tags = "secret";
        BeatmapSet s({a, b}, NOMOD);
        for (const char* q : {"title", "tt", "artist", "aa", "mapper", "game src", "normal", "345", "foo bar", "hell insane", "secret", "", "r"})
            CHECK(s.matches(q));
        CHECK(!s.matches("zzz"));
        CHECK(!s.matches("TITLE"));  // the query is expected lower-case; fields are lower-cased
        // only the first beatmap's title / artist / creator / source are searched
        b.title = "OnlyInSecond"; BeatmapSet t({mk("p", 1), b}, NOMOD);
        CHECK(!t.matches("onlyinsecond"));
    }

    // ---- matches(type, op, value) ----
    {
        Beatmap a = mk("a", 3.456, 8.f); a.approachRate = 9.f; a.circleSize = 4.f; a.HPDrainRate = 6.f;
        a.bpmMax = 180; a.endTime = 123999;
        Beatmap b = mk("b", 5.0, 3.f); b.approachRate = 5.f; b.circleSize = 3.f; b.HPDrainRate = 2.f; b.bpmMax = 90; b.endTime = 30000;
        BeatmapSet s({a, b}, NOMOD);
        CHECK(s.matches("ar", "=", 9.f) && s.matches("ar", "==", 5.f) && !s.matches("ar", "=", 7.f));
        CHECK(s.matches("ar", ">", 8.f) && !s.matches("ar", ">", 9.f) && s.matches("ar", ">=", 9.f));
        CHECK(s.matches("ar", "<", 6.f) && !s.matches("ar", "<", 5.f) && s.matches("ar", "<=", 5.f));
        CHECK(s.matches("cs", "=", 3.f) && s.matches("od", "=", 8.f) && s.matches("hp", "=", 2.f));
        CHECK(s.matches("bpm", ">=", 180.f) && !s.matches("bpm", ">", 180.f));
        CHECK(s.matches("length", "=", 123.f) && s.matches("length", "=", 30.f) && !s.matches("length", "=", 124.f));  // endTime / 1000 (int)
        CHECK(s.matches("star", "=", 3.46f) && s.matches("stars", "=", 5.f) && !s.matches("stars", "=", 3.456f));
        CHECK(!s.matches("foo", "=", 1.f));   // unknown type
        CHECK(!s.matches("ar", "!=", 9.f));   // unknown operator
        CHECK(!s.matches("AR", "=", 9.f));    // case sensitive
        // star uses the raw starRating, not the mod stars
        Beatmap::ModParams dt = Beatmap::ModParams::of(0.f, 1.5f, 1.f, 0.f);
        Beatmap c = mk("c", 2.0); c.setModStars(dt, 7.0, dt);
        CHECK(BeatmapSet({c}, dt).matches("star", "=", 2.f));
    }

    // ---- favourites / played ----
    {
        BeatmapSet s({mk("a", 1), mk("b", 2)}, NOMOD);
        CHECK(!s.isFavorite() && !s.isPlayed());
        std::vector<std::string> updated;
        BeatmapSet::setFavoriteStatusHook([&](const Beatmap& m) { updated.push_back(m.version + (m.favorite ? "+" : "-")); });
        s.setFavorite(true);
        CHECK(s.isFavorite() && updated == (std::vector<std::string>{"a+", "b+"}));
        s.get(0).favorite = false;
        CHECK(s.isFavorite());  // any beatmap
        s.setFavorite(false);
        CHECK(!s.isFavorite() && updated.size() == 4);
        BeatmapSet::setFavoriteStatusHook(nullptr);
        s.setFavorite(true);    // no hook: no crash, flag still set
        CHECK(s.isFavorite() && updated.size() == 4);
        s.get(1).playCount = 1;
        CHECK(s.isPlayed());
    }

    if (failures == 0) std::printf("batch29 (BeatmapSet): all tests passed\n");
    return failures ? 1 : 0;
}
