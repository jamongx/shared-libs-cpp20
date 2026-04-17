// test_pgpool.cpp – unit + integration tests for PgResult / PgConnection /
// PgGuard / PgConnectionPool
//
// Integration tests require a live PostgreSQL instance.
// Set PGPOOL_TEST_CONNSTR before running, e.g.:
//   export PGPOOL_TEST_CONNSTR="host=localhost dbname=testdb user=testuser password=secret"
//   ./build/test_pgpool
// Tests that need a real database will SKIP automatically if the env var is not set.

#ifdef PDK_HAS_POSTGRESQL

#include <atomic>
#include <cstdlib>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "pgpool.h"

using namespace pdk;

// ── Helper ────────────────────────────────────────────────────────────────────

static const char* live_connstr()
{
    return std::getenv("PGPOOL_TEST_CONNSTR");  // nullptr if not set
}

// Returns a connstr that fails quickly (port 1 → connection refused).
static constexpr const char* kBadConnstr =
    "hostaddr=127.0.0.1 port=1 dbname=x connect_timeout=1";

// ── PgResult – unit tests (no database needed) ────────────────────────────────

TEST(PgResult, DefaultConstructedIsInvalid)
{
    PgResult r;
    EXPECT_EQ(r.status(), PGRES_FATAL_ERROR);
    EXPECT_FALSE(r.ok());
    EXPECT_FALSE(r.command_ok());
    EXPECT_EQ(r.ntuples(), 0);
    EXPECT_EQ(r.nfields(), 0);
    EXPECT_STREQ(r.value(0, 0), "");
    EXPECT_STREQ(r.error_message(), "");
    EXPECT_EQ(r.get(), nullptr);
}

TEST(PgResult, MoveConstructor)
{
    PgResult a;
    PgResult b(std::move(a));
    EXPECT_EQ(b.get(), nullptr);
}

TEST(PgResult, MoveAssignment)
{
    PgResult a;
    PgResult b;
    b = std::move(a);
    EXPECT_EQ(b.get(), nullptr);
    EXPECT_FALSE(b.ok());
}

// ── PgConnection – unit tests (no database needed) ────────────────────────────

TEST(PgConnection, BadConnstrThrows)
{
    EXPECT_THROW(PgConnection conn(kBadConnstr), pdk::Exception);
}

// ── Integration test fixture ──────────────────────────────────────────────────

class PgConnectionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        if (!live_connstr())
            GTEST_SKIP() << "PGPOOL_TEST_CONNSTR not set";
    }
};

TEST_F(PgConnectionTest, ConnectsSuccessfully)
{
    PgConnection conn(live_connstr());
    EXPECT_TRUE(conn.is_valid());
}

TEST_F(PgConnectionTest, ExecSelectOne)
{
    PgConnection conn(live_connstr());
    PgResult r = conn.exec("SELECT 1 AS val");
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(r.ntuples(), 1);
    EXPECT_EQ(r.nfields(), 1);
    EXPECT_STREQ(r.value(0, 0), "1");
}

TEST_F(PgConnectionTest, ExecParams)
{
    PgConnection conn(live_connstr());
    PgResult r = conn.exec_params("SELECT $1::int + $2::int", {"3", "4"});
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(r.ntuples(), 1);
    EXPECT_STREQ(r.value(0, 0), "7");
}

TEST_F(PgConnectionTest, ExecInvalidSqlFails)
{
    PgConnection conn(live_connstr());
    PgResult r = conn.exec("SELECT * FROM _pdk_nonexistent_table_xyz_");
    EXPECT_FALSE(r.ok());
    EXPECT_GT(strlen(r.error_message()), 0u);
}

TEST_F(PgConnectionTest, ResetRestoresConnection)
{
    PgConnection conn(live_connstr());
    ASSERT_TRUE(conn.is_valid());
    conn.reset();
    EXPECT_TRUE(conn.is_valid());
}

// ── PgConnectionPool integration tests ───────────────────────────────────────

class PgPoolTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        if (!live_connstr())
            GTEST_SKIP() << "PGPOOL_TEST_CONNSTR not set";
        connstr_ = live_connstr();
    }
    std::string connstr_;
};

TEST_F(PgPoolTest, ConstructionAndPoolSize)
{
    PgConnectionPool pool(connstr_, 3);
    EXPECT_EQ(pool.pool_size(), 3);
}

TEST_F(PgPoolTest, ConstructionFailsOnBadConnstr)
{
    EXPECT_THROW(PgConnectionPool pool(kBadConnstr, 1), pdk::Exception);
}

TEST_F(PgPoolTest, AcquireAndQuery)
{
    PgConnectionPool pool(connstr_, 2);
    {
        auto g = pool.acquire();
        ASSERT_TRUE(static_cast<bool>(g));
        PgResult r = g->exec("SELECT 42 AS answer");
        EXPECT_TRUE(r.ok());
        EXPECT_STREQ(r.value(0, 0), "42");
    }  // PgGuard destructor → pool.release()

    // After release, acquiring again must succeed.
    auto g2 = pool.acquire();
    EXPECT_TRUE(static_cast<bool>(g2));
}

TEST_F(PgPoolTest, TryAcquireSucceedsWhenFree)
{
    PgConnectionPool pool(connstr_, 1);
    auto og = pool.try_acquire();
    ASSERT_TRUE(og.has_value());
    EXPECT_TRUE(static_cast<bool>(*og));
}

TEST_F(PgPoolTest, TryAcquireFailsWhenExhausted)
{
    PgConnectionPool pool(connstr_, 1);
    auto g = pool.acquire();           // holds the only connection
    auto none = pool.try_acquire();    // non-blocking → should be empty
    EXPECT_FALSE(none.has_value());
}

TEST_F(PgPoolTest, AcquireTimeoutThrows)
{
    PgConnectionPool pool(connstr_, 1);
    auto g = pool.acquire();           // holds the only connection
    EXPECT_THROW(pool.acquire(50), pdk::Exception);
}

TEST_F(PgPoolTest, GuardMoveTransfersOwnership)
{
    PgConnectionPool pool(connstr_, 1);
    auto g1 = pool.acquire();
    auto g2 = std::move(g1);

    EXPECT_FALSE(static_cast<bool>(g1));   // source is now empty
    EXPECT_TRUE(static_cast<bool>(g2));    // new owner has the connection

    // Pool still exhausted (g2 holds the only connection).
    EXPECT_FALSE(pool.try_acquire().has_value());
}

TEST_F(PgPoolTest, GuardMoveAssignmentReleasesOld)
{
    PgConnectionPool pool(connstr_, 2);
    auto g1 = pool.acquire();
    auto g2 = pool.acquire();

    // Moving g2 → g1 should release g1's connection before taking g2's.
    g1 = std::move(g2);
    EXPECT_FALSE(static_cast<bool>(g2));
    EXPECT_TRUE(static_cast<bool>(g1));

    // One connection was released by the move-assign; pool should have 1 free.
    auto g3 = pool.try_acquire();
    EXPECT_TRUE(g3.has_value());
}

TEST_F(PgPoolTest, ConcurrentAcquireRelease)
{
    constexpr int kPoolSize = 3;
    constexpr int kThreads  = 9;  // 3× pool size → each conn reused 3 times

    PgConnectionPool pool(connstr_, kPoolSize);
    std::atomic<int> success_count{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([&]() {
            try {
                auto g = pool.acquire(5000);
                PgResult r = g->exec("SELECT pg_sleep(0.02)");
                if (r.ok())
                    ++success_count;
            } catch (const std::exception& e) {
                ADD_FAILURE() << "Thread threw: " << e.what();
            }
        });
    }

    for (auto& t : workers)
        t.join();

    EXPECT_EQ(success_count.load(), kThreads);
}

TEST_F(PgPoolTest, AllConnectionsReturnedAfterConcurrentUse)
{
    constexpr int kPoolSize = 2;
    PgConnectionPool pool(connstr_, kPoolSize);

    {
        std::vector<std::thread> workers;
        for (int i = 0; i < kPoolSize * 2; ++i) {
            workers.emplace_back([&]() {
                auto g = pool.acquire(3000);
                g->exec("SELECT 1");
            });
        }
        for (auto& t : workers)
            t.join();
    }

    // All connections must have been returned; pool should be fully free now.
    auto g1 = pool.try_acquire();
    auto g2 = pool.try_acquire();
    EXPECT_TRUE(g1.has_value());
    EXPECT_TRUE(g2.has_value());
    EXPECT_FALSE(pool.try_acquire().has_value());  // fully exhausted again
}

#else  // PDK_HAS_POSTGRESQL not defined

#include <gtest/gtest.h>

TEST(PgPool, SkippedNoPosgreSQLSupport)
{
    GTEST_SKIP() << "PDK_HAS_POSTGRESQL not defined – pgpool not compiled";
}

#endif  // PDK_HAS_POSTGRESQL
