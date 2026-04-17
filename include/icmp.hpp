// icmp.h – ICMP monitor (C++17)
#pragma once

#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include "sock.hpp"
#include "qthread.hpp"
#include "util.hpp"

namespace pdk {

#define MAXPACKET (65536 - 60 - 8)
#define DEFDATALEN (64 - 8)

#ifndef ENTRY_LENGTH
#define ENTRY_LENGTH 64
#endif

#ifndef DIFFTIME
#define DIFFTIME(a, b) (((b).tv_sec - (a).tv_sec) * 1000 + ((b).tv_usec - (a).tv_usec) / 1000)
#endif

enum IPPROTO_TYPE { PROTO_RAW, PROTO_ICMP };

// ── RawSocket ────────────────────────────────────────────────────────────────
class RawSocket : public Socket {
public:
    bool open(IPPROTO_TYPE nType = PROTO_ICMP)
    {
        available_ = false;
        proto_type_ = nType;
        if (nType == PROTO_RAW) {
            if (!create_socket(AF_INET, SOCK_RAW, IPPROTO_RAW))
                throw Exception::format("socket creation error:%d:%s", errno, strerror(errno));
#ifdef IP_HDRINCL
            int one = 1;
            setsockopt(socket_fd(), IPPROTO_IP, IP_HDRINCL, static_cast<void*>(&one), sizeof(one));
#endif
        } else {
            struct protoent* proto = getprotobyname("icmp");
            if (!proto)
                throw Exception::format("Unknown protocol: icmp.");
            if (!create_socket(AF_INET, SOCK_RAW, proto->p_proto))
                throw Exception::format("socket creation error:%d:%s", errno, strerror(errno));
        }
        available_ = true;
        return true;
    }
    bool bind(const char* pszAddr) { return Socket::bind(0, pszAddr); }
    bool available() { return available_; }
    IPPROTO_TYPE proto_type() { return proto_type_; }

protected:
    bool available_{false};
    IPPROTO_TYPE proto_type_{PROTO_ICMP};
};

// ── IcmpManager ──────────────────────────────────────────────────────────────
class IcmpManager : public Thread {
public:
    IcmpManager();
    ~IcmpManager() override;

    QThread* owner() { return owner_; }
    int send_icmp();
    bool start(QThread* pOwner,
               const char* pszAddr,
               int32_t priority,
               const char* pszBind,
               PCallBack aNotify = nullptr);
    bool is_active() { return active_; }
    void set_delay(int32_t msec) { delay_ = msec; }
    int32_t delay() { return delay_; }
    int32_t priority() { return priority_; }
    const char* addr() { return addr_.c_str(); }
    const char* bind_addr() { return bind_addr_.c_str(); }
    void reset_link();

    PCallBack OnSendNotify{nullptr};

protected:
    int check_sum(uint16_t* addr, int nLen);
    void* thread_proc() override;

    struct timeval time_val_ {};
    std::unique_ptr<RawSocket> sock_;
    std::unique_ptr<RawSocket> recv_sock_;
    QThread* owner_{nullptr};
    bool active_{false};
    int32_t delay_{1000};
    int32_t priority_{1};
    std::string addr_;
    std::string bind_addr_;
    Logger* logger_;
};

inline IcmpManager::IcmpManager()
    : sock_(std::make_unique<RawSocket>()),
      recv_sock_(std::make_unique<RawSocket>()),
      logger_(Logger::get())
{}

inline IcmpManager::~IcmpManager()
{
    close();
}

inline void IcmpManager::reset_link()
{
    recv_sock_.reset();
    sock_ = std::make_unique<RawSocket>();
    sock_->open(PROTO_RAW);
}

inline bool IcmpManager::start(
    QThread* pOwner, const char* pszAddr, int32_t priority, const char* pszBind, PCallBack aNotify)
{
    owner_ = pOwner;
    addr_ = pszAddr ? pszAddr : "";
    priority_ = priority;
    OnSendNotify = aNotify;

    try {
        sock_->open(PROTO_RAW);
        recv_sock_->open();
        if (pszBind) {
            bind_addr_ = pszBind;
            if (!recv_sock_->bind(pszBind)) {
                logger_->log(__FILE__,
                             __LINE__,
                             Logger::Error,
                             "bind error(Addr:%s,Err:%d-%s)",
                             pszBind,
                             errno,
                             strerror(errno));
                recv_sock_.reset();
            }
        } else {
            bind_addr_ = addr_;
        }
    } catch (const Exception& e) {
        logger_->log(Logger::Critical, "Raw[ICMP] Socket: %s", e.what());
        sock_.reset();
        recv_sock_.reset();
    }
    if (!sock_)
        return false;
    bool bRet = create();
    if (bRet)
        send_icmp();
    return bRet;
}

inline int IcmpManager::check_sum(uint16_t* addr, int nLen)
{
    int nLeft = nLen, nSum = 0;
    uint16_t* w = addr;
    uint16_t answer = 0;
    for (; nLeft > 1; nLeft -= 2)
        nSum += *w++;
    if (nLeft == 1) {
        *reinterpret_cast<uint8_t*>(&answer) = *reinterpret_cast<uint8_t*>(w);
        nSum += answer;
    }
    nSum = (nSum >> 16) + (nSum & 0xffff);
    nSum += (nSum >> 16);
    return static_cast<int>(static_cast<uint16_t>(~nSum));
}

inline int IcmpManager::send_icmp()
{
    if (!sock_)
        return -1;
    if (OnSendNotify && !OnSendNotify(static_cast<void*>(this)))
        return 1;

    uint8_t Buffer[MAXPACKET]{};
    int nLen = DEFDATALEN + 8;

    struct ip* ip_hdr = nullptr;
    struct icmp* pPacket = nullptr;

    if (sock_->proto_type() == PROTO_RAW) {
        ip_hdr = reinterpret_cast<struct ip*>(Buffer);
        pPacket = reinterpret_cast<struct icmp*>(ip_hdr + 1);
        ip_hdr->ip_v = 4;
        ip_hdr->ip_hl = 5;
        ip_hdr->ip_len = htons(static_cast<uint16_t>(sizeof(struct ip) + nLen));
        ip_hdr->ip_id = htons(static_cast<uint16_t>(getpid()));
        ip_hdr->ip_ttl = 255;
        ip_hdr->ip_p = IPPROTO_ICMP;
        ip_hdr->ip_src.s_addr = inet_addr(bind_addr_.c_str());
        ip_hdr->ip_dst.s_addr = inet_addr(addr_.c_str());
        ip_hdr->ip_sum = static_cast<uint16_t>(
            check_sum(reinterpret_cast<uint16_t*>(ip_hdr), static_cast<int>(sizeof(struct ip))));
    } else {
        pPacket = reinterpret_cast<struct icmp*>(Buffer);
    }

    pPacket->icmp_type = ICMP_ECHO;
    pPacket->icmp_code = 0;
    pPacket->icmp_cksum = 0;
    pPacket->icmp_seq = static_cast<uint16_t>(sock_->socket_fd());
    pPacket->icmp_id = static_cast<uint16_t>(getpid());
    gettimeofday(reinterpret_cast<struct timeval*>(pPacket->icmp_data), nullptr);
    memcpy(&time_val_, pPacket->icmp_data, sizeof(struct timeval));
    pPacket->icmp_cksum =
        static_cast<uint16_t>(check_sum(reinterpret_cast<uint16_t*>(pPacket), nLen));

    int nSent;
    if (sock_->proto_type() == PROTO_RAW) {
        struct sockaddr_in sock {};
        sock.sin_family = AF_INET;
        sock.sin_addr = ip_hdr->ip_dst;
        nLen = ntohs(ip_hdr->ip_len);
        nSent = sendto(sock_->socket_fd(),
                       Buffer,
                       nLen,
                       0,
                       reinterpret_cast<struct sockaddr*>(&sock),
                       sizeof(sock));
    } else {
        nSent = sock_->send_to(static_cast<void*>(Buffer), nLen, 0, addr_.c_str());
    }

    if (nSent != nLen)
        logger_->log(Logger::Error,
                     "Cannot send ICMP packet[%s] Sent=%d, nLen=%d",
                     addr_.c_str(),
                     nSent,
                     nLen);
    return nSent;
}

inline void* IcmpManager::thread_proc()
{
    int nResult, nLen;
    char Packet[MAXPACKET];
    struct timeval TimeVal;

    while (!do_exit()) {
        nResult = recv_sock_->select(delay_);
        if (nResult == -1) {
            active_ = false;
            logger_->log(Logger::Error, "ICMP Generator #%d, %s", errno, strerror(errno));
            break;
        }
        if (!nResult) {
            active_ = false;
            continue;
        }
        nLen = recv_sock_->recv(Packet, MAXPACKET);
        if (nLen < 1) {
            if (errno != EINTR)
                logger_->log(Logger::Error, "Error reading ICMP data from %s", addr_.c_str());
            break;
        }
        int nHeaderLen = (reinterpret_cast<struct ip*>(Packet)->ip_hl) << 2;
        struct icmp* icmp = reinterpret_cast<struct icmp*>(Packet + nHeaderLen);
        if (nLen < nHeaderLen + ICMP_MINLEN) {
            logger_->log(Logger::Error, "Received short packet from %s", addr_.c_str());
            break;
        }
        if (icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == static_cast<uint16_t>(getpid()) &&
            icmp->icmp_seq == static_cast<uint16_t>(sock_->socket_fd())) {
            gettimeofday(&TimeVal, nullptr);
            active_ = true;
        }
        if (DIFFTIME(time_val_, TimeVal) > delay_)
            active_ = false;
    }
    return nullptr;
}

// ── IcmpMonitor ──────────────────────────────────────────────────────────────
class IcmpMonitor : public QThread {
public:
    IcmpMonitor();
    ~IcmpMonitor() override;

    using QThread::start;

    bool start(const char* pszAddrs, PCallBack aNotify = nullptr);
    bool add_manager(const char* pszAddr, PCallBack aNotify = nullptr);
    bool remove_manager(const char* pszAddr);
    void reset_managers();
    void set_interval(int32_t msec) { interval_ = msec; }
    int32_t interval() { return interval_; }
    void set_delay(int32_t msec);
    int32_t delay() { return delay_; }

    std::unordered_map<std::string, std::unique_ptr<IcmpManager>>& managers() { return managers_; }

    bool start_timer(int msec, PDK8U which = 0);
    bool stop_timer(PDK8U which = 0);

    static bool send_notify(void* self);

    PCallBack2 OnNotify{nullptr};
    PCallBack OnAliveNotify{nullptr};

protected:
    void* thread_proc() override;

    std::unordered_map<std::string, std::unique_ptr<IcmpManager>> managers_;
    int32_t interval_{500};
    int32_t delay_{1000};
    Logger* logger_;
};

inline IcmpMonitor::IcmpMonitor() : logger_(Logger::get()) {}

inline IcmpMonitor::~IcmpMonitor()
{
    OnNotify = nullptr;
    OnAliveNotify = nullptr;
    stop();
}

inline bool IcmpMonitor::add_manager(const char* pszAddr, PCallBack aNotify)
{
    std::vector<std::string> info;
    tokenize(pszAddr, info, ":");
    int32_t priority = (info.size() == 1) ? 1 : atoi(info[1].c_str());
    const char* pszBind = (info.size() == 3) ? info[2].c_str() : nullptr;

    auto pMgr = std::make_unique<IcmpManager>();
    bool bResult = pMgr->start(this, info[0].c_str(), priority, pszBind, aNotify);
    managers_[pszAddr] = std::move(pMgr);
    return bResult;
}

inline bool IcmpMonitor::remove_manager(const char* pszAddr)
{
    auto it = managers_.find(pszAddr);
    if (it != managers_.end()) {
        managers_.erase(it);
        logger_->log(Logger::Info, "ICMPManager[%s] Removed", pszAddr);
        return true;
    }
    logger_->log(Logger::Info, "ICMPManager[%s] Not Found", pszAddr);
    return false;
}

inline void IcmpMonitor::reset_managers()
{
    for (auto& [key, mgr] : managers_)
        mgr->reset_link();
}

inline bool IcmpMonitor::start(const char* pszAddrs, PCallBack aNotify)
{
    if (getuid()) {
        logger_->log(Logger::Critical, "Can only be used by \"root\"");
        return false;
    }
    std::vector<std::string> addrs;
    tokenize(pszAddrs, addrs, " ");
    for (const auto& addr : addrs) {
        add_manager(addr.c_str(), aNotify);
        logger_->log(Logger::Info, "Host Addr[%s]", addr.c_str());
    }
    return create();
}

inline bool IcmpMonitor::start_timer(int msec, PDK8U which)
{
    return ::start_timer(this, msec, which);
}

inline bool IcmpMonitor::stop_timer(PDK8U which)
{
    return ::stop_timer(this, which);
}

inline void IcmpMonitor::set_delay(int32_t msec)
{
    delay_ = msec;
    for (auto& [key, mgr] : managers_)
        mgr->set_delay(msec);
}

inline void* IcmpMonitor::thread_proc()
{
    start_timer(interval_);
    while (!do_exit()) {
        PEVENTINFO pEvent = remove();
        if (!pEvent)
            break;

        if (EVENTINFO_TYPE(pEvent) == CI_TIMER_DONE) {
            if (OnNotify)
                OnNotify(static_cast<void*>(this), static_cast<void*>(&managers_));
            if (OnAliveNotify)
                OnAliveNotify(static_cast<void*>(this));

            for (auto& [key, mgr] : managers_)
                mgr->send_icmp();

            start_timer(interval_);
        }
    }
    return nullptr;
}

}  // namespace pdk
