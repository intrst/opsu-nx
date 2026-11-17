#include "SqliteConnection.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <variant>

#include "sqlite3.h"

namespace opsu {
namespace {

std::string sqliteMessage(sqlite3* db, int rc) {
    std::string m = "SQLite error " + std::to_string(rc) + " (" + sqlite3_errstr(rc) + ")";
    if (db != nullptr) {
        const char* e = sqlite3_errmsg(db);
        if (e != nullptr && std::strcmp(e, sqlite3_errstr(rc)) != 0) m += ": " + std::string(e);
    }
    return m;
}

/** State shared by a connection and everything created from it. sqlite3_close_v2 turns the handle into a "zombie" while statements
 *  are still alive: they stay valid until they are finalised, so destruction order never matters. */
struct Db {
    sqlite3* h = nullptr;
    std::atomic<bool> closed{false};
    std::atomic<bool> autoCommit{true};
    /** Serialises "run an update + read how many rows it changed": sqlite3_total_changes is per connection, so two threads updating
     *  through the same connection would otherwise see each other's rows in their counts. */
    std::mutex dmlMutex;

    void closeNow() {
        if (closed.exchange(true)) return;
        sqlite3_close_v2(h);
    }
    ~Db() { closeNow(); }

    sqlite3* open() const {
        if (closed.load()) throw SQLException("Connection is closed");
        return h;
    }
    [[noreturn]] void fail(int rc, const std::string& what, const std::string& sql = std::string()) const {
        std::string m = what + ": " + sqliteMessage(closed.load() ? nullptr : h, rc);
        if (!sql.empty()) m += " [SQL: " + sql + "]";
        throw SQLException(m);
    }
    void exec(const char* sql) const {
        char* err = nullptr;
        int rc = sqlite3_exec(open(), sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::string m = err ? err : sqlite3_errstr(rc);
            sqlite3_free(err);
            throw SQLException("SQLite error " + std::to_string(rc) + ": " + m + " [SQL: " + sql + "]");
        }
    }
};

/** A compiled statement, finalised when the last owner (statement object or result set) goes away. */
struct Compiled {
    sqlite3_stmt* st = nullptr;
    explicit Compiled(sqlite3_stmt* s) : st(s) {}
    Compiled(const Compiled&) = delete;
    Compiled& operator=(const Compiled&) = delete;
    ~Compiled() { sqlite3_finalize(st); }
};

std::shared_ptr<Compiled> compile(const std::shared_ptr<Db>& db, const std::string& sql) {
    sqlite3* h = db->open();
    sqlite3_stmt* st = nullptr;
    int rc = sqlite3_prepare_v2(h, sql.c_str(), static_cast<int>(sql.size()), &st, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(st);
        db->fail(rc, "Could not prepare statement", sql);
    }
    // an empty / comment-only string compiles to a null statement (JDBC would run nothing)
    return std::make_shared<Compiled>(st);
}

class ResultSetImpl : public SqlResultSet {
public:
    ResultSetImpl(std::shared_ptr<Db> db, std::shared_ptr<Compiled> c, std::string sql)
        : db_(std::move(db)), c_(std::move(c)), sql_(std::move(sql)) {}
    ~ResultSetImpl() override { close(); }

    bool next() override {
        if (!c_) throw SQLException("ResultSet is closed");
        if (done_) return false;
        db_->open();
        if (c_->st == nullptr) { done_ = true; return false; }
        int rc = sqlite3_step(c_->st);
        if (rc == SQLITE_ROW) { hasRow_ = true; return true; }
        hasRow_ = false;
        done_ = true;
        if (rc != SQLITE_DONE) db_->fail(rc, "Could not read next row", sql_);
        return false;
    }
    std::optional<std::string> getString(int col) override {
        int i = check(col);
        if (sqlite3_column_type(c_->st, i) == SQLITE_NULL) { wasNull_ = true; return std::nullopt; }
        wasNull_ = false;
        const unsigned char* p = sqlite3_column_text(c_->st, i);
        int n = sqlite3_column_bytes(c_->st, i);
        return std::string(reinterpret_cast<const char*>(p), p == nullptr ? 0 : static_cast<size_t>(n));
    }
    int getInt(int col) override { int i = check(col); mark(i); return sqlite3_column_int(c_->st, i); }
    int64_t getLong(int col) override { int i = check(col); mark(i); return sqlite3_column_int64(c_->st, i); }
    double getDouble(int col) override { int i = check(col); mark(i); return sqlite3_column_double(c_->st, i); }
    float getFloat(int col) override { return static_cast<float>(getDouble(col)); }
    bool getBoolean(int col) override { return getInt(col) != 0; }
    int8_t getByte(int col) override { return static_cast<int8_t>(getInt(col)); }
    bool wasNull() override { return wasNull_; }
    void close() override {
        if (!c_) return;
        if (c_->st != nullptr) sqlite3_reset(c_->st);  // prepared statements are re-used; owned ones are finalised by ~Compiled
        c_.reset();
        hasRow_ = false;
    }

private:
    int check(int col) {
        if (!c_) throw SQLException("ResultSet is closed");
        db_->open();
        if (!hasRow_ || c_->st == nullptr) throw SQLException("No current row in the ResultSet");
        if (col < 1 || col > sqlite3_column_count(c_->st)) throw SQLException("Column index out of range: " + std::to_string(col));
        return col - 1;
    }
    void mark(int i) { wasNull_ = sqlite3_column_type(c_->st, i) == SQLITE_NULL; }

    std::shared_ptr<Db> db_;
    std::shared_ptr<Compiled> c_;
    std::string sql_;
    bool hasRow_ = false, done_ = false, wasNull_ = false;
};

class StatementImpl : public SqlStatement {
public:
    explicit StatementImpl(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    bool execute(const std::string& sql) override {
        auto c = compile(db_, sql);
        if (c->st == nullptr) return false;
        int rc = sqlite3_step(c->st);
        if (rc == SQLITE_ROW) {
            // e.g. PRAGMA journal_mode: the statement did its work at the first step; the rows are not needed here
            sqlite3_reset(c->st);
            return true;
        }
        if (rc != SQLITE_DONE) db_->fail(rc, "Could not execute statement", sql);
        return false;
    }
    int executeUpdate(const std::string& sql) override {
        auto c = compile(db_, sql);
        if (c->st == nullptr) return 0;
        sqlite3* h = db_->open();
        std::lock_guard<std::mutex> lock(db_->dmlMutex);
        int before = sqlite3_total_changes(h);
        int rc;
        while ((rc = sqlite3_step(c->st)) == SQLITE_ROW) {}
        if (rc != SQLITE_DONE) db_->fail(rc, "Could not execute update", sql);
        return sqlite3_total_changes(h) - before;
    }
    std::unique_ptr<SqlResultSet> executeQuery(const std::string& sql) override {
        return std::make_unique<ResultSetImpl>(db_, compile(db_, sql), sql);
    }
    void close() override {}

private:
    std::shared_ptr<Db> db_;
};

using Param = std::variant<std::monostate, int64_t, double, std::string>;

class PreparedImpl : public SqlPreparedStatement {
public:
    PreparedImpl(std::shared_ptr<Db> db, const std::string& sql) : db_(std::move(db)), sql_(sql), c_(compile(db_, sql)) {
        params_.resize(c_->st == nullptr ? 0 : static_cast<size_t>(sqlite3_bind_parameter_count(c_->st)));
    }

    void setString(int i, const std::string& v) override { set(i, Param(v)); }
    void setNull(int i) override { set(i, Param(std::monostate{})); }
    void setInt(int i, int v) override { set(i, Param(static_cast<int64_t>(v))); }
    void setLong(int i, int64_t v) override { set(i, Param(v)); }
    void setDouble(int i, double v) override { set(i, Param(v)); }
    void setFloat(int i, float v) override { set(i, Param(static_cast<double>(v))); }
    void setBoolean(int i, bool v) override { set(i, Param(static_cast<int64_t>(v ? 1 : 0))); }
    void setByte(int i, int8_t v) override { set(i, Param(static_cast<int64_t>(v))); }

    int executeUpdate() override {
        ensureOpen();
        return runOne(params_);
    }
    std::unique_ptr<SqlResultSet> executeQuery() override {
        ensureOpen();
        bind(params_);
        return std::make_unique<ResultSetImpl>(db_, c_, sql_);
    }
    void addBatch() override { ensureOpen(); batch_.push_back(params_); }
    std::vector<int> executeBatch() override {
        ensureOpen();
        std::vector<std::vector<Param>> work;
        work.swap(batch_);  // JDBC clears the batch, also on failure
        std::vector<int> counts;
        counts.reserve(work.size());
        for (const auto& p : work) counts.push_back(runOne(p));
        return counts;
    }
    void clearBatch() override { batch_.clear(); }
    void close() override { c_.reset(); batch_.clear(); }

private:
    void ensureOpen() {
        if (!c_) throw SQLException("PreparedStatement is closed");
        db_->open();
    }
    void set(int i, Param v) {
        if (!c_) throw SQLException("PreparedStatement is closed");
        if (i < 1 || static_cast<size_t>(i) > params_.size()) throw SQLException("Parameter index out of range: " + std::to_string(i));
        params_[static_cast<size_t>(i) - 1] = std::move(v);
    }
    void bind(const std::vector<Param>& ps) {
        if (c_->st == nullptr) return;
        sqlite3_reset(c_->st);
        sqlite3_clear_bindings(c_->st);
        for (size_t k = 0; k < ps.size(); k++) {
            int idx = static_cast<int>(k) + 1, rc;
            const Param& p = ps[k];
            if (std::holds_alternative<int64_t>(p)) rc = sqlite3_bind_int64(c_->st, idx, std::get<int64_t>(p));
            else if (std::holds_alternative<double>(p)) rc = sqlite3_bind_double(c_->st, idx, std::get<double>(p));
            else if (std::holds_alternative<std::string>(p)) {
                const std::string& s = std::get<std::string>(p);
                rc = sqlite3_bind_text64(c_->st, idx, s.data(), s.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
            } else rc = sqlite3_bind_null(c_->st, idx);
            if (rc != SQLITE_OK) db_->fail(rc, "Could not bind parameter " + std::to_string(idx), sql_);
        }
    }
    int runOne(const std::vector<Param>& ps) {
        if (c_->st == nullptr) return 0;
        bind(ps);
        sqlite3* h = db_->open();
        std::lock_guard<std::mutex> lock(db_->dmlMutex);
        int before = sqlite3_total_changes(h);
        int rc;
        while ((rc = sqlite3_step(c_->st)) == SQLITE_ROW) {}
        int changes = sqlite3_total_changes(h) - before;
        sqlite3_reset(c_->st);
        if (rc != SQLITE_DONE) db_->fail(rc, "Could not execute prepared statement", sql_);
        return changes;
    }

    std::shared_ptr<Db> db_;
    std::string sql_;
    std::shared_ptr<Compiled> c_;
    std::vector<Param> params_;
    std::vector<std::vector<Param>> batch_;
};

class ConnectionImpl : public SqlConnection {
public:
    explicit ConnectionImpl(std::shared_ptr<Db> db) : db_(std::move(db)) {}
    ~ConnectionImpl() override { close(); }

    std::unique_ptr<SqlStatement> createStatement() override { db_->open(); return std::make_unique<StatementImpl>(db_); }
    std::unique_ptr<SqlPreparedStatement> prepareStatement(const std::string& sql) override {
        return std::make_unique<PreparedImpl>(db_, sql);
    }
    bool getAutoCommit() override { db_->open(); return db_->autoCommit.load(); }
    void setAutoCommit(bool ac) override {
        db_->open();
        if (db_->autoCommit.load() == ac) return;
        db_->exec(ac ? "COMMIT;" : "BEGIN;");
        db_->autoCommit = ac;
    }
    void commit() override {
        db_->open();
        if (db_->autoCommit.load()) throw SQLException("Database in auto-commit mode");
        db_->exec("COMMIT;");
        db_->exec("BEGIN;");
    }
    void rollback() override {
        db_->open();
        if (db_->autoCommit.load()) throw SQLException("Database in auto-commit mode");
        db_->exec("ROLLBACK;");
        db_->exec("BEGIN;");
    }
    void close() override { db_->closeNow(); }
    bool isClosed() override { return db_->closed.load(); }

private:
    std::shared_ptr<Db> db_;
};

}  // namespace

std::unique_ptr<SqlConnection> openSqliteConnection(const std::string& path, const SqliteOpenOptions& options) {
    auto db = std::make_shared<Db>();
    int flags = SQLITE_OPEN_FULLMUTEX | (options.readOnly ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
    int rc = sqlite3_open_v2(path.c_str(), &db->h, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::string m = "Could not open database '" + path + "': " + sqliteMessage(db->h, rc);
        sqlite3_close_v2(db->h);  // a handle is returned even on failure
        db->h = nullptr;
        db->closed = true;
        throw SQLException(m);
    }
    sqlite3_busy_timeout(db->h, options.busyTimeoutMs);
    return std::make_unique<ConnectionImpl>(std::move(db));
}

SqlConnectionFactory sqliteConnectionFactory(const SqliteOpenOptions& options) {
    return [options](const std::string& path) { return openSqliteConnection(path, options); };
}

const char* sqliteLibraryVersion() { return sqlite3_libversion(); }

}  // namespace opsu
