#ifdef PDK_HAS_POSTGRESQL

#include "pgpool.hpp"
#include "logger.hpp"

namespace pdk {

// ── PgConnection ──────────────────────────────────────────────────────────────

PgConnection::PgConnection(const std::string& connstr) : connstr_(connstr)
{
    conn_ = PQconnectdb(connstr_.c_str());
    if (!is_valid()) {
        std::string err = conn_ ? PQerrorMessage(conn_) : "out of memory";
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
        throw Exception::format("PgConnection: connect failed – %s", err.c_str());
    }
}

PgConnection::~PgConnection()
{
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool PgConnection::is_valid() const noexcept
{
    return conn_ && PQstatus(conn_) == CONNECTION_OK;
}

void PgConnection::reset()
{
    if (conn_)
        PQreset(conn_);
}

PgResult PgConnection::exec(const char* sql)
{
    return PgResult(PQexec(conn_, sql));
}

PgResult PgConnection::exec_params(const char* sql, const std::vector<std::string>& params)
{
    std::vector<const char*> vals;
    vals.reserve(params.size());
    for (const auto& p : params)
        vals.push_back(p.c_str());

    return PgResult(PQexecParams(
        conn_,
        sql,
        static_cast<int>(vals.size()),
        nullptr,                              // paramTypes: let server infer
        vals.empty() ? nullptr : vals.data(),
        nullptr,                              // paramLengths: text mode
        nullptr,                              // paramFormats: text mode
        0                                     // resultFormat: text
    ));
}

// ── PgGuard ───────────────────────────────────────────────────────────────────

void PgGuard::release()
{
    if (conn_ && pool_) {
        pool_->release(conn_);
        conn_ = nullptr;
        pool_ = nullptr;
    }
}

// ── PgConnectionPool ──────────────────────────────────────────────────────────

PgConnectionPool::PgConnectionPool(const std::string& connstr, int pool_size)
    : connstr_(connstr),
      pool_size_(pool_size),
      available_slots_(static_cast<unsigned>(pool_size))
{
    all_connections_.reserve(pool_size_);
    for (int i = 0; i < pool_size_; ++i) {
        all_connections_.push_back(std::make_unique<PgConnection>(connstr_));
        available_pool_.push_back(all_connections_.back().get());
    }
}

PgGuard PgConnectionPool::acquire(long timeout_ms)
{
    if (!available_slots_.wait(timeout_ms))
        throw Exception::format(
            "PgConnectionPool::acquire: timed out after %ld ms", timeout_ms);

    AutoLock lk(&pool_lock_);
    PgConnection* conn = available_pool_.front();
    available_pool_.pop_front();
    return PgGuard(conn, this);
}

std::optional<PgGuard> PgConnectionPool::try_acquire()
{
    if (!available_slots_.wait(0))
        return std::nullopt;

    AutoLock lk(&pool_lock_);
    PgConnection* conn = available_pool_.front();
    available_pool_.pop_front();
    return PgGuard(conn, this);
}

void PgConnectionPool::release(PgConnection* conn)
{
    if (!conn->is_valid()) {
        conn->reset();
        if (!conn->is_valid()) {
            // Attempt to rebuild the broken connection in place.
            for (auto& owned : all_connections_) {
                if (owned.get() == conn) {
                    try {
                        owned = std::make_unique<PgConnection>(connstr_);
                        conn = owned.get();
                        Logger::get()->log(Logger::Info,
                            "PgConnectionPool: rebuilt broken connection");
                    } catch (const std::exception& e) {
                        Logger::get()->log(Logger::Error,
                            "PgConnectionPool: failed to rebuild connection – %s", e.what());
                        // Return the broken connection to pool; next acquire will try again.
                        conn = owned.get();
                    }
                    break;
                }
            }
        }
    }

    {
        AutoLock lk(&pool_lock_);
        available_pool_.push_back(conn);
    }
    available_slots_.signal();
}

}  // namespace pdk

#endif  // PDK_HAS_POSTGRESQL
