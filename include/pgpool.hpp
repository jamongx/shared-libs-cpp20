#pragma once

#ifdef PDK_HAS_POSTGRESQL

#include <libpq-fe.h>

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base.hpp"
#include "jthread.hpp"

namespace pdk {

// ── PgResult – PGresult* RAII wrapper ─────────────────────────────────────────
// Calls PQclear() automatically; move-only.
class PgResult {
public:
    explicit PgResult(PGresult* res = nullptr) noexcept : res_(res) {}

    ~PgResult()
    {
        if (res_)
            PQclear(res_);
    }

    PgResult(const PgResult&) = delete;
    PgResult& operator=(const PgResult&) = delete;

    PgResult(PgResult&& o) noexcept : res_(o.res_) { o.res_ = nullptr; }
    PgResult& operator=(PgResult&& o) noexcept
    {
        if (this != &o) {
            if (res_)
                PQclear(res_);
            res_ = o.res_;
            o.res_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] ExecStatusType status() const noexcept
    {
        return res_ ? PQresultStatus(res_) : PGRES_FATAL_ERROR;
    }
    [[nodiscard]] bool ok() const noexcept
    {
        auto s = status();
        return s == PGRES_TUPLES_OK || s == PGRES_COMMAND_OK;
    }
    [[nodiscard]] bool command_ok() const noexcept { return status() == PGRES_COMMAND_OK; }
    [[nodiscard]] int ntuples() const noexcept { return res_ ? PQntuples(res_) : 0; }
    [[nodiscard]] int nfields() const noexcept { return res_ ? PQnfields(res_) : 0; }
    [[nodiscard]] const char* value(int row, int col) const noexcept
    {
        return res_ ? PQgetvalue(res_, row, col) : "";
    }
    [[nodiscard]] const char* error_message() const noexcept
    {
        return res_ ? PQresultErrorMessage(res_) : "";
    }
    [[nodiscard]] PGresult* get() const noexcept { return res_; }

private:
    PGresult* res_{nullptr};
};

// ── PgConnection – PGconn* RAII wrapper ──────────────────────────────────────
// Connects in constructor; throws Exception on failure.
class PgConnection {
public:
    explicit PgConnection(const std::string& connstr);
    ~PgConnection();

    PgConnection(const PgConnection&) = delete;
    PgConnection& operator=(const PgConnection&) = delete;
    PgConnection(PgConnection&&) = delete;
    PgConnection& operator=(PgConnection&&) = delete;

    [[nodiscard]] bool is_valid() const noexcept;
    void reset();

    PgResult exec(const char* sql);
    PgResult exec_params(const char* sql, const std::vector<std::string>& params);

    [[nodiscard]] PGconn* get() const noexcept { return conn_; }
    [[nodiscard]] const std::string& connstr() const noexcept { return connstr_; }

private:
    std::string connstr_;
    PGconn* conn_{nullptr};
};

// ── PgConnectionPool (forward declaration for PgGuard) ───────────────────────
class PgConnectionPool;

// ── PgGuard – scoped auto-release (mirrors AutoSemaphore pattern) ─────────────
// Destructor calls pool_->release(conn_); move-only.
class PgGuard {
public:
    PgGuard(PgConnection* conn, PgConnectionPool* pool) noexcept
        : conn_(conn), pool_(pool)
    {}
    ~PgGuard() { release(); }

    PgGuard(const PgGuard&) = delete;
    PgGuard& operator=(const PgGuard&) = delete;

    PgGuard(PgGuard&& o) noexcept : conn_(o.conn_), pool_(o.pool_)
    {
        o.conn_ = nullptr;
        o.pool_ = nullptr;
    }
    PgGuard& operator=(PgGuard&& o) noexcept
    {
        if (this != &o) {
            release();
            conn_ = o.conn_;
            pool_ = o.pool_;
            o.conn_ = nullptr;
            o.pool_ = nullptr;
        }
        return *this;
    }

    PgConnection* operator->() const noexcept { return conn_; }
    PgConnection& operator*() const noexcept { return *conn_; }
    explicit operator bool() const noexcept { return conn_ != nullptr; }

private:
    void release();

    PgConnection* conn_{nullptr};
    PgConnectionPool* pool_{nullptr};
};

// ── PgConnectionPool ──────────────────────────────────────────────────────────
// Creates pool_size connections at construction time.
// acquire() blocks until a connection is free (or times out).
// release() is called automatically by PgGuard's destructor.
class PgConnectionPool : public Object {
public:
    PgConnectionPool(const std::string& connstr, int pool_size);
    ~PgConnectionPool() override = default;

    // Blocking acquire; throws Exception if no connection available within timeout_ms.
    PgGuard acquire(long timeout_ms = 5000);

    // Non-blocking acquire; returns std::nullopt if no connection is immediately free.
    std::optional<PgGuard> try_acquire();

    // Called by PgGuard::~PgGuard(). Validates connection before returning to pool.
    void release(PgConnection* conn);

    [[nodiscard]] int pool_size() const noexcept { return pool_size_; }

private:
    std::string connstr_;
    int pool_size_;
    Semaphore available_slots_;
    Mutex pool_lock_;
    std::vector<std::unique_ptr<PgConnection>> all_connections_;
    std::list<PgConnection*> available_pool_;
};

}  // namespace pdk

#endif  // PDK_HAS_POSTGRESQL
