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

// ── CircularQueue<TYPE,MAX> ──────────────────────────────────────────────────
#define QNODE_FREE 0
#define QNODE_USE 1
#define QNODE_SUCCESS 0
#define QNODE_BUFFER_FULL (-1)
#define QNODE_BUFFER_EMPTY (-2)

template<class TYPE, int MAX>
class CircularQueue {
protected:
    struct QNode {
        TYPE data_;
        bool in_use_{false};
    };

    QNode nodes_[MAX];
    unsigned int front_{1};
    unsigned int rear_{1};
    int max_{MAX};
    Semaphore sem_put_{static_cast<unsigned>(MAX)};
    Semaphore sem_get_{0};
    Mutex crit_sec_;

    void initialize()
    {
        max_ = MAX;
        for (int i = 0; i < MAX; i++) {
            memset(&nodes_[i].data_, 0, sizeof(TYPE));
            nodes_[i].in_use_ = QNODE_FREE;
        }
        front_ = rear_ = 1;
        sem_put_.reset_count(MAX);
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

    void queue_pos(unsigned int& nFront, unsigned int& nRear)
    {
        nFront = front_;
        nRear = rear_;
    }
    int queue_length() { return max_; }
    bool empty() { return front_ == rear_; }

    int put(TYPE* data)
    {
        sem_put_.lock();
        crit_sec_.lock();
        rear_ = (rear_ + 1) % MAX;
        if (front_ == rear_) {
            rear_ = (rear_ - 1 + MAX) % MAX;
            crit_sec_.unlock();
            return QNODE_BUFFER_FULL;
        }
        int nIndex = static_cast<int>(rear_);
        crit_sec_.unlock();
        memcpy(&nodes_[nIndex].data_, data, sizeof(TYPE));
        nodes_[nIndex].in_use_ = QNODE_USE;
        sem_get_.unlock();
        return QNODE_SUCCESS;
    }

    int get(TYPE* pType)
    {
        sem_get_.lock();
        crit_sec_.lock();
        if (front_ == rear_) {
            crit_sec_.unlock();
            return QNODE_BUFFER_EMPTY;
        }
        front_ = (front_ + 1) % MAX;
        int nIndex = static_cast<int>(front_);
        crit_sec_.unlock();
        memcpy(pType, &nodes_[nIndex].data_, sizeof(TYPE));
        memset(&nodes_[nIndex].data_, 0, sizeof(TYPE));
        nodes_[nIndex].in_use_ = QNODE_FREE;
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
    static Mutex lock_;

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
