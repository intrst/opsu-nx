#pragma once
#include <climits>
#include <stdexcept>
#include <cstdint>
#include <string>
#include "opsu/beatmap/Beatmap.h"
#include "opsu/db/Sql.h"

namespace opsu {

// SQLException now lives in Sql.h (included above); the name opsu::SQLException is unchanged.

/**
 * The part of the SQLite connection that the PORTED slice of BeatmapDB uses (the prepared statements below).
 * A real SQLite implementation (the DB layer is not ported: see "Backend decision" in PORTING_NOTES.md) implements this; tests use a fake.
 * Every method may throw SQLException.
 */
class BeatmapDBStore {
public:
    virtual ~BeatmapDBStore() = default;
    /** `DELETE FROM beatmaps WHERE dir = ? AND file = ?` (deleteMapStmt); returns the number of rows deleted (PreparedStatement.executeUpdate). */
    virtual int deleteMap(const std::string& dir, const std::string& file) = 0;
    /** `DELETE FROM beatmaps WHERE dir = ?` (deleteGroupStmt); returns the number of rows deleted. */
    virtual int deleteGroup(const std::string& dir) = 0;
    /** updateSizeStmt: stores the cache size string in the 'info' table (`Integer.toString(Math.max(cacheSize, 0))`). */
    virtual void updateCacheSize(const std::string& value) = 0;
    /** setStarsStmt: `UPDATE beatmaps SET stars = ? WHERE dir = ? AND file = ?`. */
    virtual void setStars(double stars, const std::string& dir, const std::string& file) = 0;
    /** updatePlayStatsStmt: sets playCount and lastPlayed for (dir, file). */
    virtual void updatePlayStats(int playCount, int64_t lastPlayed, const std::string& dir, const std::string& file) = 0;
    /** setFavoriteStmt: sets the favorite flag for (dir, file). */
    virtual void setFavorite(bool favorite, const std::string& dir, const std::string& file) = 0;
    /** setLocalOffsetStmt: sets the local music offset for (dir, file). */
    virtual void setLocalOffset(int offset, const std::string& dir, const std::string& file) = 0;
};

/**
 * PARTIAL port of itdelatrisu.opsu.db.BeatmapDB (27 KB, static, SQLite): ONLY the two `delete` methods and the cache-size bookkeeping they
 * touch (`cacheSize`, `updateCacheSize`), plus (batch 38) the four single-row updates. Everything else (init / createDatabase / updateDatabase, insert, load,
 * getLastModifiedMap, clearDatabase, closeConnection) is NOT ported: it needs the SQLite decision.
 *
 * Seams / deviations
 *  - Java is all static with a `Connection`; this is an instance over a non-owning `BeatmapDBStore*` (the "connection"). A null store is Java's
 *    `connection == null`: both delete methods return at once. The store must outlive this object.
 *  - `delete(String dir, String file)` / `delete(String dir)` are `deleteMap` / `deleteGroup` (`delete` is a C++ keyword).
 *  - `cacheSize` starts at -1 like Java (it is loaded from the 'info' table by `init()`, not ported: use `setCacheSize` until then).
 *    `cacheSize -= rows` wraps like a Java int. If the delete statement throws, `cacheSize` is NOT changed and no size is written.
 *  - Errors go through `ErrorHandler.error(msg, e, true)` = `Download::reportError` (the shared hook, default Log::error): a failing delete is
 *    "Failed to delete beatmap entry from database." / "Failed to delete beatmap group entry from database."; a failing size update inside
 *    `updateCacheSize` is reported by ITS OWN message ("Could not update beatmap cache size.") and is not seen by the delete's catch (the
 *    decrement has already happened). Only SQLException is caught, like Java; anything else propagates.
 *
 * Wiring to BeatmapSetList (the two DB hooks): `hooks.dbDeleteDir = [&](const std::string& d) { db.deleteGroup(d); };`
 * `hooks.dbDeleteFile = [&](const std::string& d, const std::string& f) { db.deleteMap(d, f); };`
 */
class BeatmapDB {
public:
    explicit BeatmapDB(BeatmapDBStore* store = nullptr) : store_(store) {}

    /** Sets / clears the connection (nullptr = no connection). */
    void setStore(BeatmapDBStore* store) { store_ = store; }
    bool hasConnection() const { return store_ != nullptr; }

    /** Current size of the beatmap cache table (-1 until set). */
    int cacheSize() const { return cacheSize_; }
    void setCacheSize(int n) { cacheSize_ = n; }

    /** Deletes the beatmap entry from the database (Java `delete(dir, file)`). */
    void deleteMap(const std::string& dir, const std::string& file);

    /** Deletes the beatmap group entry from the database (Java `delete(dir)`). */
    void deleteGroup(const std::string& dir);

    /**
     * setStars / updatePlayStatistics / updateFavoriteStatus / updateLocalOffset (batch 38). `dir` is the NAME of the file's parent
     * directory and `file` its file name (Java `getFile().getParentFile().getName()` / `getName()`). No connection: return at once.
     * On SQLException the error is reported with Java's message; `useUnicode` is Options.useUnicodeMetadata for `beatmap.toString()`
     * in that message (Java reads the option itself). setStars formats the rating with "%.4f" (HALF_UP, java::formatFixed).
     */
    void setStars(const Beatmap& beatmap, bool useUnicode = false);
    void updatePlayStatistics(const Beatmap& beatmap, bool useUnicode = false);
    void updateFavoriteStatus(const Beatmap& beatmap, bool useUnicode = false);
    void updateLocalOffset(const Beatmap& beatmap, bool useUnicode = false);

private:
    BeatmapDBStore* store_;
    int cacheSize_ = -1;

    /** Updates the size of the beatmap cache in the 'info' table: `Math.max(cacheSize, 0)` as a string. */
    void updateCacheSize();
};

}  // namespace opsu
