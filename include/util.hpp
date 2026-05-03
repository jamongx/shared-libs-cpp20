// util.h – utility functions and classes (C++17)
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "list.hpp"
#include "sock.hpp"
#include "jthread.hpp"
#include "logger.hpp"

namespace pdk {

// ── Path / filesystem helpers ────────────────────────────────────────────────
void split_path(
    const char* pname, std::string& head, std::string& tail, char spliter = '/', int order = 0);
bool is_dir(const char* pname) noexcept;
void make_dirs(const char* pname, mode_t mode = 0777);
const char* host_ip_addr();
void tokenize(const char* pszLine, std::vector<std::string>& value, const char* delim = " \t");

// ── Sleep helpers ────────────────────────────────────────────────────────────
void micro_sleep(int usec) noexcept;
void milli_sleep(int msec) noexcept;
void Sleep(int sec) noexcept;
int kbhit();

// ── Time helpers ─────────────────────────────────────────────────────────────
time_t current_time_ms();
int current_time_sec();

// ── Network ──────────────────────────────────────────────────────────────────
int ping_check(unsigned long nAddr);

// ── MemList ─────────────────────────────────────────────────────────────────
struct MEMITEM {
    int nLength{0};
    char* pBuffer{nullptr};
    MEMITEM* pNext{nullptr};
};
using PMEMITEM = MEMITEM*;

class MemList {
public:
    explicit MemList(int nItems = 2);
    ~MemList();

    void set_items(int nItems);
    void reset();
    void add(int nLength, char* pBuffer);
    void set_serialize();
    PMEMITEM get();

protected:
    void free_all_items();

    int item_count_;
    int cur_item_count_ = 0;
    PMEMITEM head_ = nullptr;
    PMEMITEM tail_ = nullptr;
    PMEMITEM cur_ = nullptr;
};

// ── Queue return codes (legacy POD CircularQueue + modern BoundedQueue) ─────
#define QNODE_FREE 0
#define QNODE_USE 1
#define QNODE_SUCCESS 0
#define QNODE_BUFFER_FULL (-1)
#define QNODE_BUFFER_EMPTY (-2)

// ── CircularQueue<TYPE,MAX> ──────────────────────────────────────────────────
// Bounded SPSC/MPMC queue for trivially-copyable POD payloads (uses
// memcpy/memset). The capacity actually usable is MAX-1 because front==rear
// means empty. Producers blocked-on-full are released by `unlock_queue()`
// at destruction.
//
// Thread-safety: protected by `crit_sec_`; `sem_put_` and `sem_get_` provide
// blocking back-pressure. Multiple producers and consumers are supported
// (counting semaphores serialize the slot account; the mutex serializes
// front_/rear_ updates).
template<class TYPE, int MAX>
class CircularQueue {
    static_assert(MAX > 1, "CircularQueue capacity MAX must be > 1");

protected:
    struct QNode {
        TYPE data_;
        bool in_use_{false};
    };

    QNode nodes_[MAX];
    unsigned int front_{0};
    unsigned int rear_{0};
    int max_{MAX};
    // Effective capacity is MAX - 1 (one slot is sacrificed to disambiguate
    // empty vs full via front==rear). Initialise sem_put_ accordingly so the
    // semaphore count and structural capacity always agree.
    Semaphore sem_put_{static_cast<unsigned>(MAX - 1)};
    Semaphore sem_get_{0};
    std::mutex crit_sec_;

    void initialize()
    {
        max_ = MAX;
        for (int i = 0; i < MAX; i++) {
            memset(&nodes_[i].data_, 0, sizeof(TYPE));
            nodes_[i].in_use_ = QNODE_FREE;
        }
        front_ = rear_ = 0;
        sem_put_.reset_count(MAX - 1);
        // Without this, reset() on a queue with prior items would leave a
        // stale sem_get_ count and the next get() would succeed on empty.
        sem_get_.reset_count(0);
    }

public:
    CircularQueue() { initialize(); }
    virtual ~CircularQueue() { unlock_queue(); }

    void unlock_queue()
    {
        sem_put_.unlock();
        sem_get_.unlock();
    }
    void reset() { initialize(); }

    // Observers — take crit_sec_ so a concurrent put/get cannot tear the
    // front_/rear_ pair we read. Earlier revisions touched the indices
    // unprotected, which was a data race even though the result was
    // probably-correct in practice on x86. Now uniform with the producer/
    // consumer paths.
    void queue_pos(unsigned int& nFront, unsigned int& nRear)
    {
        std::scoped_lock lk(crit_sec_);
        nFront = front_;
        nRear = rear_;
    }
    int queue_length() { return max_; }   // immutable after construction
    bool empty()
    {
        std::scoped_lock lk(crit_sec_);
        return front_ == rear_;
    }

    // Blocking put. Returns QNODE_SUCCESS on enqueue.
    int put(TYPE* data) { return put_impl(data, /*nonblocking=*/false, 0); }
    int try_put(TYPE* data) { return put_impl(data, /*nonblocking=*/true, 0); }

    int get(TYPE* out) { return get_impl(out, /*nonblocking=*/false, 0); }
    int try_get(TYPE* out) { return get_impl(out, /*nonblocking=*/true, 0); }
    int get_timed(TYPE* out, int timeout_ms) {
        return get_impl(out, /*nonblocking=*/false, timeout_ms);
    }

private:
    // Shared put implementation. nonblocking=true → try-once (timeout_ms ignored);
    // otherwise blocks indefinitely (timeout_ms ignored — Semaphore::lock()).
    //
    // PUBLISH ORDERING (Codex round 3 finding): the payload memcpy and the
    // QNODE_USE marker MUST happen BEFORE rear_ is advanced, otherwise a
    // concurrent producer P2 could advance rear_ past P1's slot and signal
    // sem_get_, letting a consumer read P1's not-yet-initialised slot. We
    // therefore copy/mark FIRST while still holding crit_sec_, then advance
    // rear_, then signal sem_get_.
    int put_impl(TYPE* data, bool nonblocking, int /*timeout_ms*/)
    {
        if (nonblocking) {
            if (!sem_put_.lock(0)) return QNODE_BUFFER_FULL;
        } else {
            sem_put_.lock();
        }
        {
            std::scoped_lock lk(crit_sec_);
            unsigned int next_rear = (rear_ + 1) % MAX;
            if (next_rear == front_) {
                sem_put_.unlock();   // restore reservation, no leak
                return QNODE_BUFFER_FULL;
            }
            // Copy + mark while holding the lock so a concurrent consumer
            // cannot observe an uninitialised slot.
            int nIndex = static_cast<int>(rear_);
            memcpy(&nodes_[nIndex].data_, data, sizeof(TYPE));
            nodes_[nIndex].in_use_ = QNODE_USE;
            // Publish only after the payload is committed.
            rear_ = next_rear;
        }
        sem_get_.unlock();
        return QNODE_SUCCESS;
    }

    int get_impl(TYPE* out, bool nonblocking, int timeout_ms)
    {
        if (nonblocking) {
            if (!sem_get_.lock(0)) return QNODE_BUFFER_EMPTY;
        } else if (timeout_ms > 0) {
            if (!sem_get_.lock(static_cast<unsigned>(timeout_ms)))
                return QNODE_BUFFER_EMPTY;
        } else {
            sem_get_.lock();
        }
        {
            std::scoped_lock lk(crit_sec_);
            if (front_ == rear_) {
                sem_get_.unlock();
                return QNODE_BUFFER_EMPTY;
            }
            int nIndex = static_cast<int>(front_);
            memcpy(out, &nodes_[nIndex].data_, sizeof(TYPE));
            memset(&nodes_[nIndex].data_, 0, sizeof(TYPE));
            nodes_[nIndex].in_use_ = QNODE_FREE;
            front_ = (front_ + 1) % MAX;
        }
        sem_put_.unlock();
        return QNODE_SUCCESS;
    }
};

// ── GlobalTimer ───────────────────────────────────────────────────────────────────
#define CI_TIMER_DONE 0xFFFF
#define DIFFTIME(tvp, uvp) \
    PDK_ABS((tvp.tv_sec - uvp.tv_sec) * 1000 + (tvp.tv_usec - uvp.tv_usec) / 1000)

class QThread;

class GlobalTimer : public Thread {
public:
    GlobalTimer();
    ~GlobalTimer() override;

    static GlobalTimer* get();

    bool start_timer(QThread* obj, int msecs, int nWhich = 0);
    bool stop_timer(QThread* obj, int nWhich = 0);

protected:
    struct TimeSpec {
        struct timeval tv;
        int32_t nTerm;
        uint8_t nWhich;
        QThread* obj;
    };
    using PTimeSpec = TimeSpec*;

    void* thread_proc() override;
    bool AddElement(QThread* obj, int msecs, int nWhich = 0);
    bool RemoveElement(QThread* obj, int nWhich = 0);

    static bool initialized_;
    static GlobalTimer* instance_;
    static std::mutex lock_;

    // obj → (nWhich → PTimeSpec)
    std::unordered_map<QThread*, std::unordered_map<int, PTimeSpec>> timer_map_;
    std::list<PTimeSpec> timer_list_;
    Logger* logger_;
};

bool start_timer(QThread* obj, int msecs, int nWhich = 0);
bool stop_timer(QThread* obj, int nWhich = 0);
void stop_global_timer();

void to_lower(const char* pszstr);
void to_upper(const char* pszstr);
bool is_digit(const char* str);

}  // namespace pdk
