#pragma once
// SQLite implementation of opsu/db/Sql.h (the JDBC-like seam). This header does NOT include sqlite3.h.
//
// Behaviour copied from the sqlite-jdbc / SQLDroid driver the Java app used:
//  - getInt/getLong/getDouble on NULL return 0 and wasNull() is true; getString on NULL returns nullopt.
//  - getFloat = (float) of the stored double; setFloat stores the double of the float (0.7f -> 0.699999988079071).
//  - setBoolean stores 1/0, getBoolean is "!= 0"; setByte / setInt / setLong store integers.
//  - setAutoCommit(false) runs BEGIN, setAutoCommit(true) runs COMMIT; commit() commits and begins again when not in auto-commit mode.
//  - executeUpdate returns the number of rows the statement changed (sqlite3_total_changes difference: 0 for DDL).
//  - busy timeout 3000 ms (the driver's default).
// Opened with SQLITE_OPEN_FULLMUTEX (serialized): ONE connection may be used from several threads, but a single
// statement / result set object may not be used from two threads at the same time (see docs/agents/T5.md).
#include <memory>
#include <string>
#include "opsu/db/Sql.h"

namespace opsu {

struct SqliteOpenOptions {
    /** SQLITE_OPEN_READONLY (the file must exist; no CREATE). */
    bool readOnly = false;
    /** sqlite3_busy_timeout. */
    int busyTimeoutMs = 3000;
};

/** Opens the database file (or ":memory:"). Throws SQLException with the SQLite message. */
std::unique_ptr<SqlConnection> openSqliteConnection(const std::string& path, const SqliteOpenOptions& options = {});

/** A SqlConnectionFactory over openSqliteConnection (what main.cpp gives DBController). */
SqlConnectionFactory sqliteConnectionFactory(const SqliteOpenOptions& options = {});

/** sqlite3_libversion(), for logs and tests. */
const char* sqliteLibraryVersion();

}  // namespace opsu
