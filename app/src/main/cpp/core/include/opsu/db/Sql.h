#pragma once
// A small JDBC-like seam between the game and the SQL engine. PURE: no sqlite3.h here. The SQLite implementation is
// platform/db/SqliteConnection; tests may use fakes.
//
// Only the subset of java.sql that BeatmapDB.java / DBController.java really use (Connection, Statement, PreparedStatement,
// ResultSet). Every method may throw SQLException. Column and parameter indices are 1-based, like JDBC.
//
// Ownership / lifetime (differs from JDBC only where C++ needs it):
//  - Statements and result sets are owned by the caller (unique_ptr) and may outlive nothing they depend on in a way that crashes:
//    a result set / statement used after its connection was closed throws SQLException (or is a no-op for close()).
//  - A PreparedStatement is re-usable; its result sets share the underlying compiled statement, so only ONE result set of a
//    prepared statement may be open at a time, and executing the statement again invalidates the previous result set (same as JDBC).
//  - Not thread safe by itself: one statement / result set must not be used from two threads at once (see docs/agents/T5.md).
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace opsu {

/** java.sql.SQLException: thrown for any database problem. (Moved here from BeatmapDB.h, which still includes it.) */
class SQLException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/** java.sql.ResultSet (forward only). A column read converts like the sqlite-jdbc driver did (see SqliteConnection.h). */
class SqlResultSet {
public:
    virtual ~SqlResultSet() = default;
    /** Moves to the next row; false when there are no more rows. */
    virtual bool next() = 0;
    /** The column as text, or nullopt for SQL NULL (JDBC null). UTF-8, may contain any byte. */
    virtual std::optional<std::string> getString(int column) = 0;
    /** NULL reads as 0 (and wasNull() is then true). */
    virtual int getInt(int column) = 0;
    virtual int64_t getLong(int column) = 0;
    virtual double getDouble(int column) = 0;
    /** (float) of the stored double, like getFloat in the JDBC driver. */
    virtual float getFloat(int column) = 0;
    /** != 0. */
    virtual bool getBoolean(int column) = 0;
    /** (int8_t) of the stored integer. */
    virtual int8_t getByte(int column) = 0;
    /** Whether the LAST column read was SQL NULL. */
    virtual bool wasNull() = 0;
    virtual void close() = 0;
};

/** java.sql.Statement. */
class SqlStatement {
public:
    virtual ~SqlStatement() = default;
    /** Runs one SQL statement; true if it produced a result set (e.g. a PRAGMA that returns a row). */
    virtual bool execute(const std::string& sql) = 0;
    /** Runs one statement; returns the number of rows changed (0 for DDL). */
    virtual int executeUpdate(const std::string& sql) = 0;
    virtual std::unique_ptr<SqlResultSet> executeQuery(const std::string& sql) = 0;
    /** Hint only (no-op for SQLite). */
    virtual void setFetchSize(int) {}
    virtual void close() = 0;
};

/** java.sql.PreparedStatement. Parameters not set read as NULL. */
class SqlPreparedStatement {
public:
    virtual ~SqlPreparedStatement() = default;
    virtual void setString(int index, const std::string& value) = 0;
    virtual void setNull(int index) = 0;
    virtual void setInt(int index, int value) = 0;
    virtual void setLong(int index, int64_t value) = 0;
    virtual void setDouble(int index, double value) = 0;
    /** Stored as the double of the float (JDBC setFloat), so 0.7f is stored as 0.699999988079071. */
    virtual void setFloat(int index, float value) = 0;
    /** Stored as the integer 1 / 0. */
    virtual void setBoolean(int index, bool value) = 0;
    virtual void setByte(int index, int8_t value) = 0;
    /** Convenience: nullopt -> setNull. */
    void setNullableString(int index, const std::optional<std::string>& value) {
        if (value) setString(index, *value); else setNull(index);
    }

    /** Runs the statement; returns the rows changed. */
    virtual int executeUpdate() = 0;
    virtual std::unique_ptr<SqlResultSet> executeQuery() = 0;
    /** Copies the current parameters into the batch. */
    virtual void addBatch() = 0;
    /** Runs the batch (rows changed per entry) and clears it. Throws SQLException at the first failure (earlier entries stay applied). */
    virtual std::vector<int> executeBatch() = 0;
    virtual void clearBatch() = 0;
    virtual void close() = 0;
};

/** java.sql.Connection. */
class SqlConnection {
public:
    virtual ~SqlConnection() = default;
    virtual std::unique_ptr<SqlStatement> createStatement() = 0;
    virtual std::unique_ptr<SqlPreparedStatement> prepareStatement(const std::string& sql) = 0;
    virtual bool getAutoCommit() = 0;
    /** false starts a transaction; true commits the open one. */
    virtual void setAutoCommit(bool autoCommit) = 0;
    /** Commits (and, when not in auto-commit mode, starts the next transaction). */
    virtual void commit() = 0;
    virtual void rollback() = 0;
    /** Closes the connection. Safe to call twice. Statements still alive are finalised when they are destroyed. */
    virtual void close() = 0;
    virtual bool isClosed() = 0;
};

/**
 * Opens (creating if needed) the database file at `path`: the `DriverManager.getConnection("jdbc:sqlite:" + path)` of DBController.
 * Throws SQLException. DBController never knows which engine this is.
 */
using SqlConnectionFactory = std::function<std::unique_ptr<SqlConnection>(const std::string& path)>;

}  // namespace opsu
