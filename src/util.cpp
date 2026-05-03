// util.cpp – utility implementations (C++17)
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "util.hpp"
#include "qthread.hpp"

namespace pdk {

// ── Sleep helpers ─────────────────────────────────────────────────────────────
void micro_sleep(int usec) noexcept
{
    std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

void milli_sleep(int msec) noexcept
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

void Sleep(int sec) noexcept
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

// ── kbhit ─────────────────────────────────────────────────────────────────────
int kbhit()
{
    fd_set rdflg;
    timeval timev;
    FD_ZERO(&rdflg);
    FD_SET(STDIN_FILENO, &rdflg);
    timev.tv_sec = 0;
    timev.tv_usec = 0;
    select(1, &rdflg, nullptr, nullptr, &timev);
    return FD_ISSET(STDIN_FILENO, &rdflg);
}

// ── Time ──────────────────────────────────────────────────────────────────────
time_t current_time_ms()
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int current_time_sec()
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int>(tv.tv_sec);
}

// ── ping_check ─────────────────────────────────────────────────────────────────
int ping_check(unsigned long nAddr)
{
    char szAddr[64], szTmp[128];
    snprintf(szAddr,
             sizeof(szAddr),
             "%u.%u.%u.%u",
             static_cast<unsigned>((nAddr >> 24) & 0xff),
             static_cast<unsigned>((nAddr >> 16) & 0xff),
             static_cast<unsigned>((nAddr >> 8) & 0xff),
             static_cast<unsigned>(nAddr & 0xff));
    snprintf(szTmp, sizeof(szTmp), "/usr/local/bin/ping_another -f -q %s > /dev/null", szAddr);
    return system(szTmp) == 0 ? 1 : -1;
}

// ── MemList ──────────────────────────────────────────────────────────────────
MemList::MemList(int nItems) : item_count_(nItems) {}

MemList::~MemList()
{
    free_all_items();
}

void MemList::set_items(int nItems)
{
    free_all_items();
    item_count_ = nItems;
    cur_item_count_ = 0;
}

void MemList::reset()
{
    cur_item_count_ = 0;
    free_all_items();
}

void MemList::add(int nLength, char* pBuffer)
{
    if (cur_item_count_ == item_count_) {
        PMEMITEM pItem = (item_count_ > 1) ? head_ : tail_;
        if (item_count_ > 1) {
            head_ = head_->pNext;
            tail_->pNext = pItem;
            tail_ = pItem;
            pItem->pNext = nullptr;
        }
        if (pItem->nLength != nLength) {
            delete[] pItem->pBuffer;
            pItem->nLength = nLength;
            pItem->pBuffer = new char[nLength];
        }
        memcpy(pItem->pBuffer, pBuffer, nLength);
    } else {
        auto* pItem = new MEMITEM;
        pItem->pBuffer = new char[nLength];
        pItem->pNext = nullptr;
        pItem->nLength = nLength;
        memcpy(pItem->pBuffer, pBuffer, nLength);
        if (!head_)
            head_ = tail_ = pItem;
        else {
            tail_->pNext = pItem;
            tail_ = pItem;
        }
        cur_item_count_++;
    }
}

void MemList::set_serialize()
{
    cur_ = head_;
}

PMEMITEM MemList::get()
{
    if (!cur_)
        return nullptr;
    PMEMITEM pItem = cur_;
    cur_ = cur_->pNext;
    return pItem;
}

void MemList::free_all_items()
{
    while (head_) {
        PMEMITEM pItem = head_;
        head_ = head_->pNext;
        delete[] pItem->pBuffer;
        delete pItem;
    }
    head_ = tail_ = nullptr;
}

// ── Path helpers ──────────────────────────────────────────────────────────────
void split_path(const char* pname, std::string& head, std::string& tail, char spliter, int order)
{
    const char* strptr = order ? strchr(pname, spliter) : strrchr(pname, spliter);
    if (!strptr) {
        head = pname;
        tail = "";
        return;
    }
    head = std::string(pname, strptr - pname);
    tail = strptr + 1;
}

bool is_dir(const char* pname) noexcept
{
    struct stat buf;
    if (stat(pname, &buf) != 0)
        return false;
    return (buf.st_mode & S_IFDIR) == S_IFDIR;
}

void make_dirs(const char* pname, mode_t mode)
{
    std::string name(pname), head, tail;
    split_path(name.c_str(), head, tail);
    if (tail.empty()) {
        name = head;
        split_path(name.c_str(), head, tail);
    }
    if (!head.empty() && !tail.empty() && !is_dir(head.c_str()))
        make_dirs(head.c_str(), mode);
    mkdir(name.c_str(), mode);
}

const char* host_ip_addr()
{
    static char buf[64];
    char HostName[255] = {0};
    gethostname(HostName, sizeof(HostName));
    struct hostent* lphost = gethostbyname(HostName);
    if (lphost) {
        snprintf(buf,
                 sizeof(buf),
                 "%d.%d.%d.%d",
                 static_cast<uint8_t>(lphost->h_addr[0]),
                 static_cast<uint8_t>(lphost->h_addr[1]),
                 static_cast<uint8_t>(lphost->h_addr[2]),
                 static_cast<uint8_t>(lphost->h_addr[3]));
    }
    return buf;
}

void tokenize(const char* pszLine, std::vector<std::string>& value, const char* delim)
{
    std::string line(pszLine);
    char* saveptr = nullptr;
    char* token = strtok_r(const_cast<char*>(line.c_str()), delim, &saveptr);
    while (token) {
        value.emplace_back(token);
        token = strtok_r(nullptr, delim, &saveptr);
    }
}

// ── GlobalTimer ───────────────────────────────────────────────────────────────────

bool GlobalTimer::initialized_ = false;
GlobalTimer* GlobalTimer::instance_ = nullptr;
std::mutex GlobalTimer::lock_;

GlobalTimer::GlobalTimer() : logger_(Logger::get()) {}

GlobalTimer::~GlobalTimer()
{
    // Stop + join the timer thread BEFORE the timer_map_ teardown begins;
    // otherwise thread_proc could iterate on a half-destroyed map.
    PDK_DERIVED_THREAD_DTOR_CLOSE();
    for (auto& [obj, whichMap] : timer_map_)
        for (auto& [which, pSpec] : whichMap)
            delete pSpec;
}

GlobalTimer* GlobalTimer::get()
{
    if (!instance_) {
        instance_ = new GlobalTimer;
        instance_->create();
        while (!GlobalTimer::initialized_)
            milli_sleep(10);
    }
    return instance_;
}

bool GlobalTimer::start_timer(QThread* obj, int msecs, int nWhich)
{
    std::scoped_lock lk(lock_);
    return AddElement(obj, msecs, nWhich);
}

bool GlobalTimer::stop_timer(QThread* obj, int nWhich)
{
    std::scoped_lock lk(lock_);
    return RemoveElement(obj, nWhich);
}

bool GlobalTimer::AddElement(QThread* obj, int msecs, int nWhich)
{
    auto& whichMap = timer_map_[obj];
    if (whichMap.count(nWhich))
        return false;  // already running

    auto* pSpec = new TimeSpec;
    gettimeofday(&pSpec->tv, nullptr);
    pSpec->nTerm = msecs;
    pSpec->nWhich = static_cast<uint8_t>(nWhich);
    pSpec->obj = obj;

    whichMap[nWhich] = pSpec;
    timer_list_.push_back(pSpec);
    return true;
}

bool GlobalTimer::RemoveElement(QThread* obj, int nWhich)
{
    auto it = timer_map_.find(obj);
    if (it == timer_map_.end())
        return false;

    auto& whichMap = it->second;
    auto jt = whichMap.find(nWhich);
    if (jt == whichMap.end())
        return false;

    PTimeSpec pSpec = jt->second;
    timer_list_.remove(pSpec);
    whichMap.erase(jt);
    if (whichMap.empty())
        timer_map_.erase(it);
    delete pSpec;
    return true;
}

void* GlobalTimer::thread_proc()
{
    struct timeval ntv;
    GlobalTimer::initialized_ = true;

    while (!do_exit()) {
        gettimeofday(&ntv, nullptr);
        {
            std::scoped_lock lk(lock_);
            for (auto it = timer_list_.begin(); it != timer_list_.end();) {
                PTimeSpec pSpec = *it;
                auto objIt = timer_map_.find(pSpec->obj);
                if (objIt == timer_map_.end()) {
                    ++it;
                    continue;
                }
                auto& whichMap = objIt->second;
                auto specIt = whichMap.find(pSpec->nWhich);
                if (specIt == whichMap.end()) {
                    ++it;
                    continue;
                }

                if (DIFFTIME(pSpec->tv, ntv) >= pSpec->nTerm) {
                    QThread* obj = pSpec->obj;
                    int nWhich = pSpec->nWhich;
                    char strBuf[64];
                    snprintf(strBuf,
                             sizeof(strBuf),
                             "Term:%ld, Delay:%d",
                             static_cast<long>(DIFFTIME(pSpec->tv, ntv)),
                             pSpec->nTerm);
                    it = timer_list_.erase(it);
                    whichMap.erase(nWhich);
                    if (whichMap.empty())
                        timer_map_.erase(objIt);
                    delete pSpec;
                    obj->add_event(CI_TIMER_DONE,
                                   static_cast<PDK16U>(nWhich),
                                   strBuf,
                                   static_cast<int>(strlen(strBuf)) + 1);
                } else {
                    ++it;
                }
            }
        }
        milli_sleep(1);
    }
    return nullptr;
}

bool start_timer(QThread* obj, int msecs, int nWhich)
{
    return GlobalTimer::get()->start_timer(obj, msecs, nWhich);
}

bool stop_timer(QThread* obj, int nWhich)
{
    return GlobalTimer::get()->stop_timer(obj, nWhich);
}

void stop_global_timer()
{
    GlobalTimer::get()->close();
}

void to_lower(const char* pszstr)
{
    char* ptr = const_cast<char*>(pszstr);
    while (*ptr) {
        *ptr = static_cast<char>(tolower(*ptr));
        ptr++;
    }
}

void to_upper(const char* pszstr)
{
    char* ptr = const_cast<char*>(pszstr);
    while (*ptr) {
        *ptr = static_cast<char>(toupper(*ptr));
        ptr++;
    }
}

bool is_digit(const char* str)
{
    const char* ptr = str;
    while (*ptr)
        if (!isdigit(*ptr++))
            return false;
    return true;
}

}  // namespace pdk
