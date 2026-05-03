#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>
#include <thread>
#include <pthread.h>
#include <signal.h>
#include "base.hpp"

namespace pdk {

#define SIGNAL_CONTROL

// ── Mutex → std::mutex thin wrapper ───────────────────────────────────────
// DEPRECATED in PDK 4.2. New code should use std::mutex directly together
// with std::scoped_lock (handles multiple mutexes, deadlock-free) or
// std::lock_guard. The wrapper is kept only for legacy call sites; PDK
// internal modules still use it because they compose with `pdk::AutoLock`.
//
// Migration:
//     pdk::Mutex   m;          →  std::mutex   m;
//     pdk::AutoLock lk(&m);    →  std::scoped_lock lk(m);
class [[deprecated("PDK 4.2: use std::mutex with std::scoped_lock")]] Mutex {
public:
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    bool try_lock() { return mutex_.try_lock(); }
    std::mutex& native() noexcept { return mutex_; }

private:
    std::mutex mutex_;
};

// Suppress the deprecation warning when the type is referenced inside this
// header itself (e.g., the pdk::CircularQueue/EventQueue continues to use
// it). The user-facing diagnostic still fires at consumer call sites.
#define PDK_SILENCE_MUTEX_DEPRECATION_BEGIN \
    _Pragma("GCC diagnostic push")          \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define PDK_SILENCE_MUTEX_DEPRECATION_END \
    _Pragma("GCC diagnostic pop")

// ── AutoLock → std::lock_guard equivalent (legacy) ───────────────────────────
class [[deprecated("PDK 4.2: use std::scoped_lock with std::mutex")]] AutoLock {
public:
    PDK_SILENCE_MUTEX_DEPRECATION_BEGIN
    explicit AutoLock(Mutex* pLock) : lock_(pLock->native()) {}
    PDK_SILENCE_MUTEX_DEPRECATION_END

private:
    std::lock_guard<std::mutex> lock_;
};

// ── Semaphore → std::condition_variable + std::mutex ─────────────────────────
class Semaphore {
public:
    explicit Semaphore(unsigned value = 1) : count_(value) {}
    virtual ~Semaphore() = default;

    bool reset_count(unsigned value)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        count_ = value;
        return true;
    }

    void wait()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this] { return count_ > 0; });
        --count_;
    }
    int wait(long msecs)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        bool ok = cv_.wait_for(lk, std::chrono::milliseconds(msecs), [this] { return count_ > 0; });
        if (ok)
            --count_;
        return ok ? 1 : 0;
    }
    virtual void signal()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        ++count_;
        cv_.notify_one();
    }

    bool lock()
    {
        wait();
        return true;
    }
    bool lock(unsigned msTimeout) { return wait(static_cast<long>(msTimeout)) == 1; }
    bool unlock()
    {
        signal();
        return true;
    }
    bool unlock(long lCount, int* /*lprev*/ = nullptr)
    {
        for (long i = 0; i < lCount; ++i)
            signal();
        return true;
    }
    int will_block() const
    {
        std::unique_lock<std::mutex> lk(mutex_);
        return count_ == 0 ? 1 : 0;
    }

protected:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    unsigned count_{0};
};

// ── AutoSemaphore ─────────────────────────────────────────────────────────────
class AutoSemaphore {
public:
    explicit AutoSemaphore(Semaphore* p) : m_p(p) { m_p->wait(); }
    ~AutoSemaphore() { m_p->signal(); }

private:
    Semaphore* m_p;
};

// ── Sync / SyncAck ──────────────────────────────────────────────────────────
class Sync : public Semaphore {
public:
    Sync() : Semaphore(0) {}
};

class SyncAck : public Sync {
public:
    void signal() override
    {
        Sync::signal();
        ack_.wait();
    }
    void ack() { ack_.signal(); }

private:
    Sync ack_;
};

// ── Queue<T> – bounded mutex/cv queue, value-semantic ────────────────────────
// Cleaned-up replacement for the legacy Queue<P,T> two-template-parameter
// design. T is the element type (any movable type, including non-trivially
// copyable types like std::string).
//
// ┌────────────────────────────────────────────────────────────────────┐
// │ ⚠ SINGLE-CONSUMER CONTRACT                                         │
// │                                                                    │
// │ The pointer-returning overloads — get() and try_get() — return an  │
// │ interior pointer to the queue's `scratch_` member. The next call   │
// │ to any get/try_get OVERWRITES scratch_, so the previous pointer    │
// │ becomes invalid. This means at most ONE consumer thread may use    │
// │ the pointer overloads on a given Queue instance.                   │
// │                                                                    │
// │ Producers may be many (put/try_put are MPMC-safe). Consumers may   │
// │ also be many — but only if every consumer uses pop()/try_pop()     │
// │ which return BY VALUE (std::optional<T>) and have no aliasing      │
// │ concern.                                                           │
// │                                                                    │
// │ EventQueue (= Queue<EVENTINFO>) is single-consumer (the QThread    │
// │ that owns it), so the pointer API is appropriate there.            │
// └────────────────────────────────────────────────────────────────────┘
#define DEFAULT_QUEUESIZE 32

template<class T>
class Queue {
public:
    explicit Queue(int n = DEFAULT_QUEUESIZE) : max_(n) {}
    ~Queue() { shutdown(); }

    // Non-blocking push. Returns 0 on success, -1 if queue full or shut down.
    // MPMC-safe (no interior pointer involvement on the producer side). The
    // (unused) second argument is retained so legacy QThread call sites
    // that pass the event byte length compile unchanged.
    int put(const T* item, int /*nLen*/ = 0)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        if (closed_ || static_cast<int>(q_.size()) >= max_)
            return -1;
        q_.push(*item);
        cv_get_.notify_one();
        return 0;
    }

    // ── Single-consumer API (pointer return, returns into shared scratch) ──

    // Blocking pop. Returns interior pointer (valid until the NEXT get on
    // the same queue) or nullptr if the queue was shut down while empty.
    // SINGLE-CONSUMER ONLY — see contract note above.
    T* get()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_get_.wait(lk, [this] { return !q_.empty() || closed_; });
        if (q_.empty())
            return nullptr;
        scratch_ = std::move(q_.front());
        q_.pop();
        return &scratch_;
    }

    // Non-blocking pop. SINGLE-CONSUMER ONLY.
    int try_get(T** out)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        if (q_.empty())
            return 0;
        scratch_ = std::move(q_.front());
        q_.pop();
        *out = &scratch_;
        return 1;
    }

    // Bounded-wait pop. Timeout is in seconds for legacy compatibility.
    // SINGLE-CONSUMER ONLY.
    int try_get(T** out, int nWaitTimeoutSec)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        bool ok = cv_get_.wait_for(
            lk, std::chrono::seconds(nWaitTimeoutSec),
            [this] { return !q_.empty() || closed_; });
        if (!ok || q_.empty())
            return 0;
        scratch_ = std::move(q_.front());
        q_.pop();
        *out = &scratch_;
        return 1;
    }

    // ── MPMC-safe API (value return, no scratch_ aliasing) ──────────────

    // Blocking pop returning by value. Multiple consumers may call this
    // concurrently. nullopt only when the queue is shut down and empty.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_get_.wait(lk, [this] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;
        T value = std::move(q_.front());
        q_.pop();
        return value;
    }

    std::optional<T> try_pop() {
        std::unique_lock<std::mutex> lk(mutex_);
        if (q_.empty()) return std::nullopt;
        T value = std::move(q_.front());
        q_.pop();
        return value;
    }

    int length() {
        std::unique_lock<std::mutex> lk(mutex_);
        return static_cast<int>(q_.size());
    }

    // Wake all waiters; subsequent put() returns -1, blocked get() returns
    // nullptr once the queue drains.
    void shutdown() {
        std::unique_lock<std::mutex> lk(mutex_);
        closed_ = true;
        cv_get_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_get_;
    std::queue<T> q_;
    int max_;
    T scratch_{};   // single-consumer scratch — see contract note above
    bool closed_{false};
};

// Legacy two-template alias kept so any hold-out call sites continue to
// compile, but deprecated. The first template parameter (P) was always
// expected to be `T*`; we now ignore P entirely and forward to Queue<T>.
template<class P, class T>
using LegacyQueue [[deprecated("PDK 4.2: use pdk::Queue<T> directly")]] = Queue<T>;

// ── Queue1To1<T> ─────────────────────────────────────────────────────────────
template<class T>
class Queue1To1 {
public:
    explicit Queue1To1(int /*n*/ = DEFAULT_QUEUESIZE) {}

    void put(T object)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        q_.push(std::move(object));
        cv_.notify_one();
    }
    T get()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this] { return !q_.empty(); });
        T val = std::move(q_.front());
        q_.pop();
        return val;
    }
    int try_get(T* obj)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        if (q_.empty())
            return 0;
        *obj = std::move(q_.front());
        q_.pop();
        return 1;
    }
    int length()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        return static_cast<int>(q_.size());
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> q_;
};

// ── Thread – abstract std::jthread-based thread class (C++20) ──────────────
//
// Lifecycle:
//   - Construct subclass instance.
//   - Call start() (preferred) or create() (legacy synonym) to launch.
//   - Subclass overrides thread_proc() to do work, polling do_exit().
//   - On destruction, the base destructor calls close() as a last-resort
//     safety net. EVERY concrete derived class MUST also call close() in
//     its own destructor BEFORE its members tear down — see the warning
//     below. The PDK_DERIVED_THREAD_DTOR_CLOSE() helper macro exists so
//     subclasses can spell this consistently.
//
// ┌────────────────────────────────────────────────────────────────────┐
// │ ⚠ Why derived dtors MUST call close() first                        │
// │                                                                    │
// │ A virtual call from inside a base destructor resolves statically   │
// │ to Thread::close(), NOT the derived override. So a derived class   │
// │ that overrides close() to wake a blocked thread_proc (e.g.,        │
// │ TcpServer closes its listening socket; QThread posts a quit event) │
// │ will NOT have that wakeup called from ~Thread(). Worse, by the     │
// │ time ~Thread() runs, the derived members are already destroyed —   │
// │ if thread_proc() is still spinning, it can dereference a wrecked   │
// │ object.                                                            │
// │                                                                    │
// │ Therefore: every concrete subclass of Thread/QThread must run      │
// │     close();  // or this->close();  ← derived override resolves    │
// │ as the FIRST line of its own destructor.                           │
// └────────────────────────────────────────────────────────────────────┘
//
// Internally uses std::jthread + std::stop_source so the subclass's
// `do_exit()` polls the canonical std::stop_token. Detached operation is no
// longer supported (jthread always joins), which matches the medical-device
// posture: every worker must converge on shutdown.
//
// Subclasses that need to wake up a blocked thread_proc on shutdown should
// override close() to also signal whatever wait the thread is parked on
// (e.g., QThread posts a quit event; TcpServer closes the listening socket).
class Thread : public Object {
public:
    Thread() = default;
    ~Thread() override { close(); }

    // Preferred entry point. Returns true on successful launch. Virtual so
    // QThread can extend the start sequence.
    virtual bool start() { return create(0) == 1; }

    // Legacy synonym for start(). bDetached is now ignored (jthread always
    // joins); a non-zero value logs a warning at the implementation site.
    int create(int bDetached = 0);
    virtual void close();
    void close_pre();
    void close_post();
    int thread_exists();
    // Note: cannot be const because we acquire mutex_. The lifecycle mutex
    // serialises all thread_/stop_source_ access.
    bool joinable() noexcept {
        std::unique_lock<std::mutex> lk(mutex_);
        return thread_ && thread_->joinable();
    }

    // Underlying pthread handle. Returns 0 (invalid) when the thread has
    // not been created yet — callers must check before passing to
    // pthread_setschedparam, pthread_setaffinity_np, etc. The handle stays
    // valid for the duration of the thread; callers that race with close()
    // are responsible for not passing the returned handle after close().
    pthread_t native_handle() noexcept {
        std::unique_lock<std::mutex> lk(mutex_);
        return (thread_ && thread_->joinable()) ? thread_->native_handle() : pthread_t{};
    }

    // Direct access to the std::stop_token for cooperative cancellation.
    // Subclasses that wait on std::condition_variable_any can register the
    // token to wake on close(). The returned token remains valid even
    // after request_stop() has been called.
    std::stop_token get_stop_token() noexcept {
        std::unique_lock<std::mutex> lk(mutex_);
        return stop_source_.get_token();
    }

protected:
    virtual void* thread_proc() = 0;
    bool do_exit() noexcept {
        return stop_source_.stop_requested();
    }

#ifdef SIGNAL_CONTROL
    void sig_allow(int sig);
    void sig_prevent(int sig);
    void sig_allow_all();
    void sig_prevent_all();
#endif

private:
    // std::jthread auto-joins on destruction; we wrap in optional so we can
    // construct lazily on start() without needing a default-constructed
    // payload. stop_source_ is owned here (not the jthread's built-in one)
    // so close_pre() can request stop independently of jthread lifetime.
    // mutex_ + cv_ serialise ALL access to thread_/stop_source_/closing_.
    //
    // closing_ guards the lifecycle window between move-out-for-join and
    // join completion: while close_post() is parked in join() outside the
    // mutex, a concurrent create() must NOT see thread_ as nullopt and
    // start a fresh worker against the same stop_source_. create() blocks
    // on cv_ until closing_ flips back to false.
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<std::jthread> thread_;
    std::stop_source stop_source_;
    bool closing_{false};
};

// Helper macro that spells the required derived-destructor close() call
// uniformly. Use as the FIRST statement of every subclass destructor.
//
//     ~MyWorker() override {
//         PDK_DERIVED_THREAD_DTOR_CLOSE();
//         // ... derived cleanup ...
//     }
#define PDK_DERIVED_THREAD_DTOR_CLOSE() \
    do { this->close(); } while (0)

#ifdef SIGNAL_CONTROL
inline void Thread::sig_allow(int sig)
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, sig);
    pthread_sigmask(SIG_UNBLOCK, &ss, nullptr);
}
inline void Thread::sig_prevent(int sig)
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, sig);
    pthread_sigmask(SIG_BLOCK, &ss, nullptr);
}
inline void Thread::sig_allow_all()
{
    sigset_t ss;
    sigfillset(&ss);
    pthread_sigmask(SIG_UNBLOCK, &ss, nullptr);
}
inline void Thread::sig_prevent_all()
{
    sigset_t ss;
    sigfillset(&ss);
    sigdelset(&ss, SIGSEGV);
    sigdelset(&ss, SIGABRT);
    pthread_sigmask(SIG_BLOCK, &ss, nullptr);
}
#endif

}  // namespace pdk
