// sock.h – socket wrappers (C++17)
#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string>
#include "logger.hpp"

namespace pdk {

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

using SOCKADDR_IN = struct sockaddr_in;
using SOCKADDR = struct sockaddr;
using PDKSOCKET = int;
using UINT = unsigned int;

enum SOCKMODE { SOCK_NONE = -1, SOCK_TCP, SOCK_UDP, SOCK_UUDP };
enum SOCKRMODE { NONE, SERVER, CLIENT };
enum CONNECTSTATUS { OK, FAIL };

// ── Socket ───────────────────────────────────────────────────────────────────
class Socket : public Object {
public:
    Socket();
    ~Socket() override;

    [[nodiscard]] virtual bool open(const char* pszPath);
    [[nodiscard]] virtual bool open(UINT nPort, const char* pszAddr = nullptr);
    virtual void close();

    virtual int select(int timeout);
    virtual int select();

    [[nodiscard]] virtual int recv(void* lpBuf, int nBufLen, int nFlags = 0);
    [[nodiscard]] virtual int recv_from(
        void* lpBuf, int nBufLen, char* rSocketAddress, UINT& rSocketPort, int nFlags = 0);

    [[nodiscard]] virtual int send(const void* lpBuf, int nBufLen, int nFlags = 0);
    [[nodiscard]] virtual int send_to(const void* lpBuf,
                        int nBufLen,
                        UINT nHostPort,
                        const char* lpszHostAddress = nullptr,
                        int nFlags = 0,
                        bool bDebug = true);

    bool sock_name(char* rSocketAddress, UINT& rSocketPort);
    bool peer_name(char* rPeerAddress, UINT& rPeerPort);

    [[nodiscard]] int addr_type() const { return addr_type_; }
    [[nodiscard]] int socket_fd() const { return socket_fd_; }
    [[nodiscard]] const char* path() const { return m_path.empty() ? nullptr : m_path.c_str(); }

    operator PDKSOCKET() const { return socket_fd_; }

    bool assign_socket(int hSocket, int nAddrType);

protected:
    bool create_socket(int nAddrFormat, int nSocketType, int nProtocolType = 0);
    bool bind(const char* pszPath);
    bool bind(UINT nPort, const char* pszAddr = nullptr);

    PDKSOCKET socket_fd_{INVALID_SOCKET};
    Logger* logger_;
    std::string m_path;
    int addr_type_{AF_INET};
    int err_occurred_{0};
};

// ── TcpSocket ────────────────────────────────────────────────────────────────
class TcpSocket : public Socket {
public:
    TcpSocket();
    ~TcpSocket() override = default;

    [[nodiscard]] bool open(const char* pszPath) override;
    [[nodiscard]] bool open(UINT nPort, const char* pszAddr = nullptr) override;
    void close() override;

    [[nodiscard]] bool prepare_to_client(UINT nPort, const char* pszAddr, bool bBlock = false, int nSec = 5);
    [[nodiscard]] bool prepare_to_server(UINT nPort, const char* pszAddr, int nConnectionBacklog = 1);

    [[nodiscard]] bool listen(int nConnectionBacklog = 1);

    [[nodiscard]] bool connect(UINT nPort, const char* lpszAddr, bool bBlock = false, int nSec = 5);
    [[nodiscard]] bool connect(const SOCKADDR* lpSockAddr, int nSockAddrLen);
    [[nodiscard]] bool connect(const SOCKADDR* lpSockAddr, int nSockAddrLen, int nSec);

    [[nodiscard]] bool reconnect();
    [[nodiscard]] const char* connect_addr() const { return addr_buf_; }
    void set_env_params(int nInterval, int nRetryCount)
    {
        reconnect_interval_ = nInterval;
        reconnect_retry_count_ = nRetryCount;
    }

    bool accept(SOCKADDR* lpSockAddr, int* lpSockAddrLen);
    Socket* accept();

private:
    UINT port_{0};
    char addr_buf_[16]{};
    int reconnect_interval_{5};
    int reconnect_retry_count_{0};
    bool exit_{false};
};

// ── UnixUdp ──────────────────────────────────────────────────────────────────
class UnixUdp : public Socket {
public:
    UnixUdp() = default;
    ~UnixUdp() override = default;

    bool open(const char* pszPath) override;
    bool open(UINT nPort, const char* pszAddr = nullptr) override;

    using Socket::send_to;

    int send_to(const char* lpBuf, int nBuflen, const char* pszTo, bool bDebug = true);
    int send_to(sockaddr_un* pTo, const char* lpBuf, int nBuflen, bool bDebug = true);
    int recv_from(char* pBuf, int buflen, sockaddr_un* pFrom = nullptr);

    int recv(void* lpBuf, int nBufLen, int nFlags = 0) override
    {
        return recv_from(static_cast<char*>(lpBuf), nBufLen);
    }
    int recv_from(
        void* lpBuf, int nBufLen, char* rSocketAddress, UINT& rSocketPort, int nFlags = 0) override
    {
        return 0;
    }
    int send(const void* lpBuf, int nBufLen, int nFlags = 0) override { return 0; }
    int send_to(const void* lpBuf,
                int nBufLen,
                UINT nHostPort,
                const char* lpszHostAddress = nullptr,
                int nFlags = 0,
                bool bDebug = true) override
    {
        return Socket::send_to(lpBuf, nBufLen, nHostPort, lpszHostAddress, nFlags, bDebug);
    }
};

// ── InetUdpSocket ────────────────────────────────────────────────────────────
class InetUdpSocket : public Socket {
public:
    InetUdpSocket() = default;
    ~InetUdpSocket() override = default;

    bool open(const char* pszPath) override;
    bool open(UINT nPort, const char* pszAddr = nullptr) override;
};

}  // namespace pdk
