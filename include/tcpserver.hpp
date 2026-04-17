// tcpserver.h – TCP server with per-connection threads (C++17)
#pragma once

#include <list>
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
    TcpChannel(TcpServer* server, Socket* sock);
    ~TcpChannel() override;

    Socket* socket_fd() { return sock_; }
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
    Socket* sock_;
    TCP_STATUS status_{TCP_CONN};
    bool first_{false};
    int32_t ch_type_{0};
    Logger* logger_;
};

class TcpServer : public Thread {
public:
    TcpServer();
    ~TcpServer() override;

    void broadcast(void* Buffer, int nLen);
    void send_all(void* Buffer, int nLen);
    void close_channel(TcpChannel* channel);
    void start(const char* pszAddr, int32_t nPort);
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

    std::list<TcpChannel*> channels_;
    TcpSocket* socket_;
    std::string addr_;
    int port_{0};
    int limit_{1};
    Mutex lock_;
    bool use_event_{true};
    Logger* logger_;
};

}  // namespace pdk
