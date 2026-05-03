// udpserver.h – UDP server wrappers (C++17)
#pragma once

#include <memory>
#include <string>
#include "qthread.hpp"
#include "logger.hpp"
#include "sock.hpp"
#include "inifile.hpp"

namespace pdk {

// ── UUdpSender ───────────────────────────────────────────────────────────────
class UUdpSender : public UnixUdp {
public:
    UUdpSender();
    ~UUdpSender() override = default;

    using UnixUdp::open;
    using UnixUdp::send_to;

    [[nodiscard]] bool open(QThread* pOwner);
    [[nodiscard]] int send_to(void* pInfo, int nLen, const char* pszDest);
    [[nodiscard]] int send_to(PEVENTINFO pInfo, const char* pszDest);
    QThread* owner() { return owner_; }

protected:
    QThread* owner_{nullptr};
    Logger* logger_;
};

// ── UUdpReceiver ─────────────────────────────────────────────────────────────
class UUdpReceiver : public Thread {
public:
    UUdpReceiver();
    ~UUdpReceiver() override;

    using Thread::start;
    virtual void start(QThread* pOwner, const char* pszPath);
    void add_event(PEVENTINFO pBuffer);
    void add_event(void* pBuffer, int nLen);
    QThread* owner() { return owner_; }

    PCallBack3 OnAfterReceive;

protected:
    void* thread_proc() override;

    std::unique_ptr<UnixUdp> udp_recv_sock_;
    QThread* owner_{nullptr};
    Logger* logger_;
};

// ── UUdpServer ───────────────────────────────────────────────────────────────
class UUdpServer : public UUdpReceiver {
public:
    UUdpServer();
    ~UUdpServer() override = default;

    void start(QThread* pOwner, const char* pszPath) override;
    int send_to(PEVENTINFO pInfo, const char* pszDest);
    int send_to(void* pInfo, int nLen, const char* pszDest);

    PCallBack3 OnBeforeSend;

protected:
    std::unique_ptr<UUdpSender> udp_send_sock_;
};

// ── IUdpSender ───────────────────────────────────────────────────────────────
class IUdpSender : public InetUdpSocket {
public:
    IUdpSender();
    ~IUdpSender() override = default;

    using InetUdpSocket::open;
    using Socket::send_to;

    [[nodiscard]] bool open(QThread* pOwner);
    [[nodiscard]] int send_to(void* pInfo, int nLen, const char* pszAddr, int nPort);
    [[nodiscard]] int send_to(PEVENTINFO pInfo, const char* pszAddr, int nPort);
    QThread* owner() { return owner_; }

protected:
    QThread* owner_{nullptr};
    Logger* logger_;
};

// ── IUdpReceiver ─────────────────────────────────────────────────────────────
class IUdpReceiver : public Thread {
public:
    IUdpReceiver();
    ~IUdpReceiver() override;

    using Thread::start;
    [[nodiscard]] virtual bool start(QThread* pOwner, const char* pszAddr, int32_t nPort);
    void add_event(void* pBuffer);
    void add_event(void* pBuffer, int nLen);
    QThread* owner() { return owner_; }
    const char* addr() { return addr_.c_str(); }
    [[nodiscard]] int GetPort() const { return port_; }

    PCallBack3 OnAfterReceive;

protected:
    void* thread_proc() override;

    std::unique_ptr<InetUdpSocket> udp_recv_sock_;
    QThread* owner_{nullptr};
    std::string addr_;
    int port_{0};
    Logger* logger_;
};

// ── IUdpServer ───────────────────────────────────────────────────────────────
class IUdpServer : public IUdpReceiver {
public:
    IUdpServer();
    ~IUdpServer() override = default;

    bool start(QThread* pOwner, const char* pszAddr, int32_t nPort) override;
    int send_to(void* pBuffer, int nLen, const char* pszAddr, int nPort);
    int send_to(PEVENTINFO pInfo, const char* pszAddr, int32_t nPort);

    PCallBack3 OnBeforeSend;

protected:
    std::unique_ptr<IUdpSender> udp_send_sock_;
};

// ── UUdpIfThread ─────────────────────────────────────────────────────────────
#define CONF_RELOAD_MOUNT 0x01
#define CONF_DEBUG_LEVEL 0x02

class UUdpIfThread : public QThread {
public:
    UUdpIfThread();
    explicit UUdpIfThread(int n);
    // Explicit dtor that runs the canonical wakeup-then-join sequence
    // before any derived members tear down. The previous `= default` would
    // have left QThread::close() unreachable from a base destructor (see
    // jthread.hpp Thread dtor warning box).
    ~UUdpIfThread() override;

    using QThread::start;

    virtual void start(const char* pszPath);
    int32_t send_to(const char* pszPath, void* Buffer, int32_t& nLen);
    const char* path() { return path_.c_str(); }

protected:
    static int after_receive(void* self, void* pBuffer, void* pLen)
    {
        return static_cast<UUdpIfThread*>(self)->recv_handle(pBuffer, *static_cast<int32_t*>(pLen));
    }
    static int before_send(void* self, void* pBuffer, void* pLen)
    {
        return static_cast<UUdpIfThread*>(self)->send_handle(pBuffer, *static_cast<int32_t*>(pLen));
    }

    virtual int recv_handle(void* /*Buffer*/, int32_t /*nLen*/) { return 0; }
    virtual int send_handle(void* /*Buffer*/, int32_t /*nLen*/) { return 0; }

    void* thread_proc() override;

    std::unique_ptr<UUdpServer> udp_channel_;
    std::string path_;
    Logger* logger_;
};

}  // namespace pdk
