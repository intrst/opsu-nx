#include "opsu/beatmap/BeatmapSearch.h"
#include <regex>
#include "opsu/util/JavaCompat.h"

namespace opsu {

namespace {
bool isJavaSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\x0B' || c == '\f' || c == '\r'; }

/** query.split("\\s+") with Java's rules. */
std::vector<std::string> splitWhitespace(const std::string& s) {
    std::vector<std::string> parts;
    std::size_t start = 0, i = 0, n = s.size();
    while (i < n) {
        if (isJavaSpace(s[i])) {
            std::size_t j = i;
            while (j < n && isJavaSpace(s[j])) j++;
            parts.push_back(s.substr(start, i - start));  // a match at index 0 gives a leading ""
            start = j;
            i = j;
        } else {
            i++;
        }
    }
    if (parts.empty()) return {s};  // no match: the whole string (also for "")
    parts.push_back(s.substr(start));
    while (!parts.empty() && parts.back().empty()) parts.pop_back();  // trailing empties are removed
    return parts;
}
}  // namespace

std::string normalizeQuery(const std::optional<std::string>& query) {
    if (!query) return "";
    std::size_t b = 0, e = query->size();
    while (b < e && static_cast<unsigned char>((*query)[b]) <= ' ') b++;
    while (e > b && static_cast<unsigned char>((*query)[e - 1]) <= ' ') e--;
    std::string r = query->substr(b, e - b);
    for (char& c : r)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return r;
}

ParsedQuery parseQuery(const std::string& normalizedQuery) {
    static const std::regex pattern(R"((ar|cs|od|hp|bpm|length|stars?)(==?|>=?|<=?)((\d*\.)?\d+))");
    ParsedQuery out;
    for (const std::string& term : splitWhitespace(normalizedQuery)) {
        std::smatch m;
        if (std::regex_search(term, m, pattern))
            out.conditions.push_back({m[1].str(), m[2].str(), java::parseFloat(m[3].str())});
        else
            out.terms.push_back(term);
    }
    return out;
}

}  // namespace opsu
