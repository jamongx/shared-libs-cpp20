#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <signal.h>
#include "base.hpp"

namespace pdk {

#define SIGNAL_CONTROL

// ── Mutex → std::mutex thin wrapper (backward-compat name) ─────────────────
class Mutex {
public:
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    std::mutex& native() noexcept { return mutex_; }

private:
    std::mutex mutex_;
};

// ── AutoLock → std::lock_guard equivalent ────────────────────────────────────
class AutoLock {
public:
    explicit AutoLock(Mutex* pLock) : lock_(pLock->native()) {}

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

// ── Queue<P,T> – bounded queue; P is pointer type, T is value type ───────────
// Non-blocking put (returns -1 if full). Blocking Get.
#define DEFAULT_QUEUESIZE 32

template<class P, class T>
class Queue {
public:
    explicit Queue(int n = DEFAULT_QUEUESIZE) : max_(n) {}
    ~Queue()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        shutdown_ = true;
        cv_get_.notify_all();
    }

    int put(P object, int /*nLen*/ = 0)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        if (static_cast<int>(q_.size()) >= max_)
            return -1;
        q_.push(*object);
        cv_get_.notify_one();
        return 0;
    }

    P get()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_get_.wait(lk, [this] { return !q_.empty() || shutdown_; });
        if (q_.empty())
            return nullptr;
        scratch_ = q_.front();
        q_.pop();
        return &scratch_;
    }

    int try_get(P* obj)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        if (q_.empty())
            return 0;
        scratch_ = q_.front();
        q_.pop();
        *obj = &scratch_;
        return 1;
    }

    int try_get(P* obj, int nWaitTimeout)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        bool ok = cv_get_.wait_for(
            lk, std::chrono::seconds(nWaitTimeout), [this] { return !q_.empty() || shutdown_; });
        if (!ok || q_.empty())
            return 0;
        scratch_ = q_.front();
        q_.pop();
        *obj = &scratch_;
        return 1;
    }

    int length()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        return static_cast<int>(q_.size());
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_get_;
    std::queue<T> q_;
    int max_;
    T scratch_{};
    bool shutdown_{false};
};

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

// ── Thread – abstract std::thread-based thread class ────────────────────────
class Thread : public Object {
public:
    Thread() = default;
    ~Thread() override { close(); }

    int create(int bDetached = 0);
    virtual void close();
    void close_pre();
    void close_post();
    int thread_exists();

protected:
    virtual void* thread_proc() = 0;
    bool do_exit() noexcept { return stop_.load(std::memory_order_acquire); }

#ifdef SIGNAL_CONTROL
    void sig_allow(int sig);
    void sig_prevent(int sig);
    void sig_allow_all();
    void sig_prevent_all();
#endif

private:
    std::thread thread_;
    std::mutex mutex_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> created_{false};
};

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
