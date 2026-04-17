// sharedmemory.h – POSIX shared memory wrappers (C++17)
// Note: pthread_mutex_t and sem_t are kept for process-shared IPC.
#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "base.hpp"
#include "eventdefs.hpp"
#include "jthread.hpp"

namespace pdk {

#ifndef SHM_MODE
#define SHM_MODE 0666
#define FTOK_ID 'x'
#endif

#ifndef EVENT_QUEUE_LENGTH
#define EVENT_QUEUE_LENGTH 56
#endif

// ── SharedMemory<T> ─────────────────────────────────────────────────────────
template<class T>
class SharedMemory {
protected:
    pthread_mutex_t* mutex_ptr_{nullptr};
    std::string path_;
    int shm_id_{-1};
    int size_{-1};
    bool origin_{false};
    T* buffer_{nullptr};

    int initialize(int key)
    {
        if (size_ == -1)
            size_ = static_cast<int>(sizeof(pthread_mutex_t) + sizeof(T));
        else
            size_ += static_cast<int>(sizeof(pthread_mutex_t));

        if ((shm_id_ = shmget(key, size_, 0)) < 0) {
            if (errno != ENOENT)
                perror("shmget() failed");
            shm_id_ = shmget(key, size_, SHM_MODE | IPC_CREAT | IPC_EXCL);
            if (shm_id_ < 0)
                perror("shmget() failed");
            origin_ = false;
        } else {
            shm_id_ = shmget(key, size_, SHM_MODE);
            if (shm_id_ < 0)
                perror("shmget() failed with old id");
            origin_ = true;
        }
        return shm_id_;
    }

    int check_file()
    {
        struct stat buf;
        if (stat(path_.c_str(), &buf) != 0) {
            int nFile = open(path_.c_str(), O_RDWR | O_CREAT, SHM_MODE);
            if (nFile == -1)
                perror("shm file open error");
            else
                close(nFile);
        }
        return 0;
    }

public:
    explicit SharedMemory(int key) : size_(-1) { initialize(key); }
    explicit SharedMemory(const char* pszPath) : path_(pszPath ? pszPath : "")
    {
        check_file();
        key_t key = ftok(pszPath, FTOK_ID);
        if (key < 0)
            perror("ftok error");
        initialize(key);
    }
    SharedMemory(const char* pszPath, int nSize) : path_(pszPath ? pszPath : ""), size_(nSize)
    {
        check_file();
        key_t key = ftok(pszPath, FTOK_ID);
        if (key < 0)
            perror("ftok error");
        initialize(key);
    }
    ~SharedMemory()
    {
        unlock();
        detach();
    }

    int destroy()
    {
        if (control(IPC_RMID, nullptr) < 0) {
            perror("shmctl IPC_RMID failed");
            return -1;
        }
        if (!path_.empty())
            unlink(path_.c_str());
        return 1;
    }

    const char* path() { return path_.c_str(); }
    int shm_id() { return shm_id_; }
    int GetSize() { return size_; }
    bool origin() { return origin_; }

    T* attach(int shmflg = 0)
    {
        mutex_ptr_ = static_cast<pthread_mutex_t*>(shmat(shm_id_, nullptr, shmflg));
        // Place T immediately after the mutex in shared memory
        buffer_ = reinterpret_cast<T*>(mutex_ptr_ + 1);
        if (origin_) {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
            if (pthread_mutex_init(mutex_ptr_, &attr) != 0)
                perror("pthread_mutex_init error");
            pthread_mutexattr_destroy(&attr);
        }
        return buffer_;
    }

    int detach() { return mutex_ptr_ ? shmdt(static_cast<void*>(mutex_ptr_)) : 0; }
    T* data() { return buffer_ ? buffer_ : attach(); }
    int control(int cmd, struct shmid_ds* buf) { return shmctl(shm_id_, cmd, buf); }
    void lock()
    {
        if (mutex_ptr_)
            pthread_mutex_lock(mutex_ptr_);
    }
    void unlock()
    {
        if (mutex_ptr_)
            pthread_mutex_unlock(mutex_ptr_);
    }
};

// ── ShmQueue<T> ─────────────────────────────────────────────────────────────
template<class T>
class ShmQueue {
private:
    struct TShmBuffer {
        sem_t sem_put_;
        sem_t sem_get_;
        int max_;
        int m_nNextPut;
        int m_nNextGet;
        int m_nCurQueueLen;
        char m_szData;  // flexible start of T array
    };

    std::string path_;
    std::unique_ptr<SharedMemory<TShmBuffer>> shm_;
    TShmBuffer* shm_buffer_{nullptr};
    std::vector<T*> queue_objects_;

    void initialize(int n, bool bForce = false)
    {
        int nSize = static_cast<int>(sizeof(TShmBuffer) - sizeof(char) + sizeof(T) * n);
        shm_ = std::make_unique<SharedMemory<TShmBuffer>>(path_.c_str(), nSize);
        shm_buffer_ = shm_->data();

        if (!shm_->origin() || bForce) {
            if (sem_init(&shm_buffer_->sem_put_, 1, n) == -1)
                perror("sem_init error sem_put_");
            if (sem_init(&shm_buffer_->sem_get_, 1, 0) == -1)
                perror("sem_init error sem_get_");
            shm_buffer_->m_nNextPut = shm_buffer_->m_nNextGet = shm_buffer_->m_nCurQueueLen = 0;
            shm_buffer_->max_ = n;
        }
        if (bForce)
            return;

        queue_objects_.resize(n);
        for (int i = 0; i < n; i++)
            queue_objects_[i] = reinterpret_cast<T*>(&shm_buffer_->m_szData + sizeof(T) * i);
    }

public:
    explicit ShmQueue(const char* pszPath) : path_(pszPath ? pszPath : "")
    {
        initialize(EVENT_QUEUE_LENGTH);
    }
    ShmQueue(const char* pszPath, int n) : path_(pszPath ? pszPath : "") { initialize(n); }
    ~ShmQueue() { sem_post(&shm_buffer_->sem_get_); }

    int destroy() { return shm_->destroy(); }
    void reset() { initialize(shm_buffer_->max_, true); }

    bool put_event()
    {
        bool bRet = false;
        shm_->lock();
        if (shm_buffer_->m_nCurQueueLen > 1)
            bRet = true;
        shm_->unlock();
        sem_post(&shm_buffer_->sem_get_);
        return bRet;
    }

    int put(T* object, int nLen = 0)
    {
        if (sem_trywait(&shm_buffer_->sem_put_) == 0) {
            shm_->lock();
            int mSlot = shm_buffer_->m_nNextPut++ % shm_buffer_->max_;
            if (shm_buffer_->m_nNextPut == shm_buffer_->max_)
                shm_buffer_->m_nNextPut = 0;
            shm_buffer_->m_nCurQueueLen++;
            shm_->unlock();

            if (!nLen)
                nLen = static_cast<int>(sizeof(T));
            memcpy(queue_objects_[mSlot], object, nLen);

            sem_post(&shm_buffer_->sem_get_);
            return 0;
        }
        return -1;
    }

    T* get()
    {
        while (true) {
            sem_wait(&shm_buffer_->sem_get_);
            shm_->lock();
            if (!shm_buffer_->m_nCurQueueLen) {
                shm_->unlock();
                continue;
            }
            shm_buffer_->m_nCurQueueLen--;
            int mSlot = shm_buffer_->m_nNextGet++ % shm_buffer_->max_;
            if (shm_buffer_->m_nNextGet == shm_buffer_->max_)
                shm_buffer_->m_nNextGet = 0;
            shm_->unlock();

            if (queue_objects_.empty())
                return nullptr;
            T* object = queue_objects_[mSlot];
            sem_post(&shm_buffer_->sem_put_);
            return object;
        }
    }

    int try_get(T** obj)
    {
        if (sem_trywait(&shm_buffer_->sem_get_) == 0) {
            shm_->lock();
            shm_buffer_->m_nCurQueueLen--;
            int mSlot = shm_buffer_->m_nNextGet++ % shm_buffer_->max_;
            if (shm_buffer_->m_nNextGet == shm_buffer_->max_)
                shm_buffer_->m_nNextGet = 0;
            shm_->unlock();
            *obj = queue_objects_[mSlot];
            sem_post(&shm_buffer_->sem_put_);
            return 1;
        }
        return 0;
    }

    int try_get(T** obj, int nWaitTimeout)  // seconds
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        int nDeadline = static_cast<int>(tv.tv_sec) + nWaitTimeout;
        while (try_get(obj) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            gettimeofday(&tv, nullptr);
            if (nDeadline < static_cast<int>(tv.tv_sec))
                return 0;
        }
        return 1;
    }

    int peek(const char* pValue, int len, int offset)
    {
        if (shm_buffer_->m_nCurQueueLen <= 0)
            return 0;
        shm_->lock();
        int bOk = 0;
        for (int i = 0; i < shm_buffer_->m_nCurQueueLen; i++) {
            int mSlot = (shm_buffer_->m_nNextGet + i) % shm_buffer_->max_;
            const char* pSrc = reinterpret_cast<const char*>(queue_objects_[mSlot]);
            if (memcmp(pSrc + offset, pValue, len) == 0) {
                bOk = 1;
                break;
            }
        }
        shm_->unlock();
        return bOk;
    }

    int length() { return shm_buffer_->m_nCurQueueLen; }
};

using ShmEventQueue = ShmQueue<EVENTINFO>;

// ── ShmQThread ───────────────────────────────────────────────────────────────
class ShmQThread : public Thread {
private:
    class CongestThread : public Thread {
    public:
        explicit CongestThread(ShmEventQueue* pQueue) : queue_(pQueue) {}

    protected:
        ShmEventQueue* queue_;
        void* thread_proc() override
        {
            while (!do_exit()) {
                if (!queue_->put_event())
                    break;
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
            return nullptr;
        }
    };

    std::unique_ptr<CongestThread> cong_thread_;

public:
    explicit ShmQThread(const char* pPath)
        : queue_(std::make_unique<ShmEventQueue>(pPath, EVENT_QUEUE_LENGTH))
    {}
    ShmQThread(const char* pPath, int nBufferLen)
        : queue_(std::make_unique<ShmEventQueue>(pPath, nBufferLen))
    {}
    ~ShmQThread() = default;

    virtual bool start()
    {
        if (queue_->length()) {
            cong_thread_ = std::make_unique<CongestThread>(queue_.get());
            cong_thread_->create();
        }
        return create() == 1;
    }
    virtual void stop() { close(); }
    virtual void close()
    {
        close_pre();
        add_quit_event();
        close_post();
    }
    int destroy() { return queue_->destroy(); }

    bool add(PEVENTINFO pEvent) { return queue_->put(pEvent, EVENTINFO_LENGTH(pEvent)) == 0; }
    PEVENTINFO remove() { return queue_->get(); }
    PEVENTINFO try_remove();

    virtual bool add_event(PDK16U type, PDK16U stype, char* pValue = nullptr, int nLen = 0);
    virtual bool add_event(PDK16U type, PDK16U stype, PDK32U value);
    int length() { return queue_->length(); }

protected:
    virtual bool add_quit_event();
    std::unique_ptr<ShmEventQueue> queue_;
};

inline bool ShmQThread::add_quit_event()
{
    return add_event(CI_QUIT_THREAD, 0);
}

inline PEVENTINFO ShmQThread::try_remove()
{
    PEVENTINFO pEvent;
    return (queue_->try_get(&pEvent) == 1) ? pEvent : nullptr;
}

inline bool ShmQThread::add_event(PDK16U type, PDK16U stype, char* pValue, int nLen)
{
    EVENTINFO Event{};
    EVENTINFO_TYPE(&Event) = type;
    EVENTINFO_SUBTYPE(&Event) = stype;
    if (pValue && nLen > 0)
        memcpy(Event.Value, pValue, nLen);
    EVENTINFO_LENGTH(&Event) = static_cast<int>(EVENTINFO_HEADER_LENGTH) + nLen;
    return add(&Event);
}

inline bool ShmQThread::add_event(PDK16U type, PDK16U stype, PDK32U value)
{
    EVENTINFO Event{};
    EVENTINFO_TYPE(&Event) = type;
    EVENTINFO_SUBTYPE(&Event) = stype;
    *reinterpret_cast<PDK32U*>(&Event.Value[0]) = value;
    EVENTINFO_LENGTH(&Event) = static_cast<int>(EVENTINFO_HEADER_LENGTH) + sizeof(PDK32U);
    return add(&Event);
}

}  // namespace pdk
