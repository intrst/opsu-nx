#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "opsu/beatmap/BeatmapSet.h"

namespace opsu {

/**
 * The static search half of itdelatrisu.opsu.beatmap.BeatmapSetList (batch 30): `normalizeQuery`, the query splitter with
 * its conditional terms (ar/cs/od/hp/bpm/length/star(s) with = == > >= < <=), and `filter`. BeatmapSetList itself (node lists,
 * expand / unexpand, delete, DB id sets) is NOT ported yet; none of this touches shared state, exactly like the Java `filter`.
 *
 * Seams / deviations
 *  - Nodes are a template parameter: `getSet(node)` returns the node's `const BeatmapSet&` (BeatmapSetNode is not ported).
 *  - `latest.get() != seq` (cancellation, checked when `(i & 255) == 0`) is `cancelled()`; a cancelled search returns nullopt (Java null).
 *  - `toLowerCase` is ASCII-only (see BeatmapSet.h). `String.trim()` removes every char <= U+0020; `\s+` is Java's ASCII class
 *    [ \t\n\x0B\f\r]. `String.split` semantics kept: a positive-width match at index 0 gives a leading "" term, trailing empties are dropped.
 *  - The condition pattern uses std::regex (ECMAScript); for this pattern leftmost / greedy / ordered-alternation behaviour is the same
 *    as java.util.regex, and `find()` (unanchored) is `regex_search`.
 */
struct SearchCondition {
    std::string type, op;
    float value;
};

/** BeatmapSetList.normalizeQuery: null (nullopt) -> "", else trimmed + lower case. */
std::string normalizeQuery(const std::optional<std::string>& query);

/** The result of splitting a normalized query: plain text terms, then the conditional terms, both in query order. */
struct ParsedQuery {
    std::vector<std::string> terms;
    std::vector<SearchCondition> conditions;
};
/** The first half of BeatmapSetList.filter. A term containing a condition anywhere (find()) is a condition and not a text term. */
ParsedQuery parseQuery(const std::string& normalizedQuery);

/**
 * BeatmapSetList.filter(query, source, latest, seq): the nodes matching every text term, then every condition (terms first, like
 * Java), in source order. Returns early with the (empty) current list as soon as it is empty.
 */
template <class Node, class GetSet>
std::optional<std::vector<Node>> filterNodes(const std::string& normalizedQuery, const std::vector<Node>& source, GetSet getSet,
                                             const std::function<bool()>& cancelled = nullptr) {
    ParsedQuery q = parseQuery(normalizedQuery);
    std::vector<Node> current = source;
    auto keep = [&](const std::vector<Node>& in, auto matcher) -> std::optional<std::vector<Node>> {
        std::vector<Node> out;
        for (std::size_t i = 0, n = in.size(); i < n; i++) {
            if (cancelled && (i & 255) == 0 && cancelled()) return std::nullopt;
            if (matcher(getSet(in[i]))) out.push_back(in[i]);
        }
        return out;
    };
    for (const std::string& term : q.terms) {
        if (current.empty()) return current;
        auto r = keep(current, [&](const BeatmapSet& s) { return s.matches(term); });
        if (!r) return std::nullopt;
        current = std::move(*r);
    }
    for (const SearchCondition& c : q.conditions) {
        if (current.empty()) return current;
        auto r = keep(current, [&](const BeatmapSet& s) { return s.matches(c.type, c.op, c.value); });
        if (!r) return std::nullopt;
        current = std::move(*r);
    }
    return current;
}

}  // namespace opsu
