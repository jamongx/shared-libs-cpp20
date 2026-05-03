// tcpserver.h – TCP server with per-connection threads (C++17)
#pragma once

#include <list>
#include <memory>
#include <string>
#include "list.hpp"
#include "sock.hpp"
#include "qthread.hpp"
#include "util.hpp"
#include "inifile.hpp"

namespace pdk {

#define MSG_FUNC_KILL_SERVICE 0xff

enum TCP_STATUS {
    TCP_MASK = 0xefef,
    TCP_CONN = 0xe1e1,
    TCP_PING = 0xe3e3,
    TCP_PONG = 0xe7e7,
    TCP_DISC = 0x1010
};

enum CHANNEL_MODE { CHANNEL_STREAM, CHANNEL_PACKET };

class TcpServer;

class TcpChannel : public Thread {
public:
    // Takes ownership of `sock` via std::unique_ptr so destruction order is
    // deterministic: close() (which joins the recv thread) runs before the
    // socket is destroyed, removing the recv-on-deleted-fd race.
    TcpChannel(TcpServer* server, std::unique_ptr<Socket> sock);
    ~TcpChannel() override;

    Socket* socket_fd() { return sock_.get(); }
    TCP_STATUS status() noexcept { return status_; }
    void set_status(TCP_STATUS s) noexcept { status_ = s; }
    [[nodiscard]] int32_t type() const noexcept { return ch_type_; }
    void set_type(int32_t nType) noexcept { ch_type_ = nType; }
    [[nodiscard]] int send(void* Buffer, int nLen);
    TcpServer* owner() noexcept { return server_; }

    void* pOther{nullptr};

protected:
    void* thread_proc() override;

    TcpServer* server_;
    std::unique_ptr<Socket> sock_;
    TCP_STATUS status_{TCP_CONN};
    bool first_{false};
    int32_t ch_type_{0};
    Logger* logger_;
};

class TcpServer : public Thread {
public:
    TcpServer();
    ~TcpServer() override;

    using Thread::start;  // expose Thread::start() (no-arg) alongside the
                          // bind+listen overload below

    // Override close() to wake the blocked select() in thread_proc by closing
    // the listening socket before joining. Without this, shutdown hangs.
    void close() override;

    void broadcast(void* Buffer, int nLen);
    void send_all(void* Buffer, int nLen);
    void close_channel(TcpChannel* channel);
    // Bind to (addr, port) and start the accept loop. Returns false if bind
    // fails — callers should propagate that failure rather than silently
    // walking to the next port (which surprised many callers in v3.x).
    bool start(const char* pszAddr, int32_t nPort);
    void add_event(void* Buffer);
    void set_limit(int nCount) noexcept { limit_ = nCount; }
    [[nodiscard]] int count() const noexcept { return limit_; }
    void set_use_event(bool b) noexcept { use_event_ = b; }
    [[nodiscard]] bool use_event() const noexcept { return use_event_; }

    PCallBack3 OnAfterReceive;
    PCallBack3 OnBeforeSend;
    PCallBack OnConnected;
    PCallBack OnClosed;

protected:
    void check_channels();
    void* thread_proc() override;

    std::list<std::unique_ptr<TcpChannel>> channels_;
    std::unique_ptr<TcpSocket> socket_;
    std::string addr_;
    int port_{0};
    int limit_{1};
    std::mutex lock_;
    bool use_event_{true};
    Logger* logger_;
};

}  // namespace pdk
