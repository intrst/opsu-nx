// Tests for batch 30 (BeatmapSearch: the static search half of BeatmapSetList). Behavioural; no JDK here.
#include <cstdio>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapSearch.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

using V = std::vector<std::string>;
static const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);

static BeatmapSet mkSet(const std::string& title, float ar, int bpm, double stars, int endMs = 100000, const std::string& tags = "") {
    Beatmap b("/x/" + title + ".osu");
    b.title = title; b.artist = "art"; b.creator = "map"; b.version = "v"; b.tags = tags;
    b.approachRate = ar; b.bpmMax = bpm; b.starRating = stars; b.endTime = endMs;
    return BeatmapSet({b}, NOMOD);
}
static V names(const std::optional<std::vector<const BeatmapSet*>>& r) {
    V v; if (r) for (auto* s : *r) v.push_back(s->get(0).title); return v;
}

int main() {
    // normalizeQuery
    CHECK(normalizeQuery(std::nullopt) == "");
    CHECK(normalizeQuery(std::string("  Foo BAR \t")) == "foo bar");
    CHECK(normalizeQuery(std::string("\x01\x02" "AbC\x1f")) == "abc");  // trim removes everything <= U+0020
    CHECK(normalizeQuery(std::string("   ")) == "");

    // parseQuery: splitting
    { auto p = parseQuery("a  b\tc"); CHECK(p.terms == (V{"a", "b", "c"}) && p.conditions.empty()); }
    { auto p = parseQuery(" a"); CHECK(p.terms == (V{"", "a"})); }            // Java split: leading "" for a width>0 match at 0
    { auto p = parseQuery("a "); CHECK(p.terms == (V{"a"})); }                // trailing empties dropped
    { auto p = parseQuery(""); CHECK(p.terms == (V{""})); }                   // no match: the whole string
    { auto p = parseQuery("\x0B" "a\f" "b"); CHECK(p.terms == (V{"", "a", "b"})); }  // \x0B and \f are \s
    // conditions
    {
        auto p = parseQuery("ar>=9 foo stars<4.5 bpm==180 length<=.5 cs=4 od>3 hp<1 star=5");
        CHECK(p.terms == (V{"foo"}));
        CHECK(p.conditions.size() == 8);
        CHECK(p.conditions[0].type == "ar" && p.conditions[0].op == ">=" && p.conditions[0].value == 9.f);
        CHECK(p.conditions[1].type == "stars" && p.conditions[1].op == "<" && p.conditions[1].value == 4.5f);
        CHECK(p.conditions[2].op == "==" && p.conditions[2].value == 180.f);
        CHECK(p.conditions[3].type == "length" && p.conditions[3].value == 0.5f);
        CHECK(p.conditions[4].op == "=" && p.conditions[5].op == ">" && p.conditions[6].op == "<");
        CHECK(p.conditions[7].type == "star" && p.conditions[7].value == 5.f);
    }
    // find(), not matches(): the pattern may sit inside a term
    { auto p = parseQuery("xar>=9y"); CHECK(p.terms.empty() && p.conditions.size() == 1 && p.conditions[0].value == 9.f); }
    { auto p = parseQuery("ar>=9.5.5"); CHECK(p.conditions.size() == 1 && p.conditions[0].value == 9.5f); }
    { auto p = parseQuery("ar>=x ar9 ar> ar>=.  bpm>>5"); CHECK(p.conditions.empty() && p.terms.size() == 5); }
    { auto p = parseQuery("ar=>5"); CHECK(p.conditions.empty() && p.terms.size() == 1); }  // "=>" : '=' then '>' is not a digit
    { auto p = parseQuery("hp<=1x2"); CHECK(p.conditions.size() == 1 && p.conditions[0].op == "<=" && p.conditions[0].value == 1.f); }

    // filter
    std::vector<BeatmapSet> sets;
    sets.push_back(mkSet("alpha", 9.f, 180, 5.0, 200000, "fast"));
    sets.push_back(mkSet("alpine", 7.f, 120, 3.0, 90000));
    sets.push_back(mkSet("beta", 9.5f, 200, 6.2, 150000, "fast"));
    std::vector<const BeatmapSet*> src;
    for (auto& s : sets) src.push_back(&s);
    auto get = [](const BeatmapSet* s) -> const BeatmapSet& { return *s; };
    auto run = [&](const std::string& q, std::function<bool()> c = nullptr) { return filterNodes(normalizeQuery(q), src, get, c); };

    CHECK(names(run("alp")) == (V{"alpha", "alpine"}));
    CHECK(names(run("ALP fast")) == (V{"alpha"}));              // terms are AND
    CHECK(names(run("ar>=9")) == (V{"alpha", "beta"}));
    CHECK(names(run("fast ar>9")) == (V{"beta"}));              // terms, then conditions
    CHECK(names(run("ar>=9 bpm<200")) == (V{"alpha"}));         // conditions are AND
    CHECK(names(run("stars>=5 length>=200")) == (V{"alpha"}));
    CHECK(names(run("zzz ar>=9")).empty() && run("zzz ar>=9").has_value());
    CHECK(names(run("alp zzz fast")).empty());
    CHECK(names(run("a ar==9.5")) == (V{"beta"}));
    CHECK(names(run("beta  ")) == (V{"beta"}));
    CHECK(names(run("x ar=>5")).empty());                        // "ar=>5" is a text term
    {   // empty source: early return with the empty list
        std::vector<const BeatmapSet*> none;
        auto r = filterNodes(std::string("a"), none, get);
        CHECK(r && r->empty());
    }
    // cancellation: checked at i = 0, 256, 512 ...; nullopt = Java null
    CHECK(!run("alp", [] { return true; }).has_value());
    CHECK(run("alp", [] { return false; }).has_value());
    {
        std::vector<const BeatmapSet*> big;
        for (int i = 0; i < 600; i++) big.push_back(&sets[0]);
        int calls = 0;
        auto r = filterNodes(std::string("alp"), big, get, [&] { return ++calls == 3; });  // 3rd check (i = 512) cancels
        CHECK(!r.has_value() && calls == 3);
        calls = 0;
        auto r2 = filterNodes(std::string("alp"), big, get, [&] { calls++; return false; });
        CHECK(r2 && r2->size() == 600 && calls == 3);            // i = 0, 256, 512 for ONE term
        calls = 0;
        auto r3 = filterNodes(std::string("alp ar>=1"), big, get, [&] { calls++; return false; });
        CHECK(r3 && calls == 6);                                  // each term / condition pass checks again
    }
    {   // the first element (i = 0) is already a cancellation point; i = 1 is not
        std::vector<const BeatmapSet*> one{&sets[0]};
        CHECK(!filterNodes(std::string("alp"), one, get, [] { return true; }).has_value());
        int calls = 0;
        std::vector<const BeatmapSet*> two{&sets[0], &sets[1]};
        filterNodes(std::string("alp"), two, get, [&] { calls++; return false; });
        CHECK(calls == 1);
    }
    // a node type other than a pointer works too
    {
        std::vector<int> idx{0, 1, 2};
        auto r = filterNodes(std::string("fast"), idx, [&](int i) -> const BeatmapSet& { return sets[i]; });
        CHECK(r && *r == (std::vector<int>{0, 2}));
    }

    if (failures == 0) std::printf("batch30 (BeatmapSearch): all tests passed\n");
    return failures ? 1 : 0;
}
