#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace opsu {
namespace java {

/**
 * The slice of java.text.SimpleDateFormat that the download servers use (parse a fixed pattern, then print "d MMM yyyy HH:mm:ss").
 *
 * parseDate(): lenient SimpleDateFormat.parse for patterns made of y/M/d/H/m/s/S, literals and 'quoted' text. Pattern letters only mean
 * what the servers use: `yyyy` (>= 3 y = the year literally), `MM` / `M` (numeric month), and one or two letters of d H m s, `SSS`/`S` (milliseconds,
 * a plain number, not a fixed width). Anything else (MMM, yy, E, a, z, ...) throws std::invalid_argument, because it was never ported.
 * Behaviour kept from the JDK: leading spaces / tabs are skipped before each number, a number may carry a '-' sign, numbers are not width limited
 * (so "99" as a month rolls into the next year, "-5" counts backwards), text after the last pattern element is ignored, the first mismatch fails.
 * Returns the instant as milliseconds since the epoch, or nullopt where Java threw ParseException.
 *   utc = true  : fields are UTC (the servers that call `f.setTimeZone(UTC)`)
 *   utc = false : fields are in the default time zone (the TZ environment variable here; Bloodcat parses like this)
 *
 * formatDate(): `new SimpleDateFormat("d MMM yyyy HH:mm:ss", Locale.US).format(new Date(ms))` in the default time zone.
 */
std::optional<int64_t> parseDate(const std::string& text, const std::string& pattern, bool utc);
std::string formatDate(int64_t epochMillis);

/** `try { fmt.format(f.parse(s)) } catch (ParseException e) { return s; }`: the whole formatDate(String) of the servers. */
std::string reformatDate(const std::string& s, const std::string& pattern, bool utc);

}  // namespace java
}  // namespace opsu
