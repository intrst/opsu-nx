#pragma once
#include <string>

namespace opsu {

/** Project-wide constants. Port of itdelatrisu.opsu.OpsuConstants. */
namespace OpsuConstants {

inline constexpr const char* PROJECT_NAME = "opsu!";
inline constexpr const char* PROJECT_AUTHOR = "@itdelatrisu + @fluddokt + @4resonance";

// Java used java.net.URI; plain strings are enough in C++ (hand them to the platform's URL opener).
inline constexpr const char* WEBSITE_URI = "https://itdelatrisu.github.io/opsu/";
inline constexpr const char* REPOSITORY_URI = "https://github.com/fluddokt/opsu";
inline constexpr const char* CREDITS_URI = "https://github.com/fluddokt/opsu/blob/master/CREDITS.md";

/** printf-style template: issue title, issue body. */
inline constexpr const char* ISSUES_URL = "https://github.com/fluddokt/opsu/issues/new?title=%s&body=%s";
inline constexpr const char* VERSION_REMOTE = "https://raw.githubusercontent.com/itdelatrisu/opsu/gh-pages/version";

/** Percent-encodes a string like java.net.URLEncoder (UTF-8, space -> '+'). */
std::string urlEncode(const std::string& s);

/** Returns the changelog URI for the given version. */
std::string getChangelogURI(const std::string& version);

}  // namespace OpsuConstants
}  // namespace opsu
