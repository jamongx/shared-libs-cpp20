// sock.cpp – socket implementations (C++17)
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/un.h>
#include "sock.hpp"
#include "util.hpp"

namespace pdk {

#define RETRY_COUNT 1
#define WAIT_TIMEOUT 100
#define SOCKET_SND_RCV_BUFFER 16384

// ── Socket ───────────────────────────────────────────────────────────────────
Socket::Socket() : logger_(Logger::get()) {}

Socket::~Socket()
{
    close();
}

bool Socket::open(const char* pszPath)
{
    m_path = pszPath ? pszPath : "";

    if (!create_socket(AF_UNIX, SOCK_DGRAM, 0))
        return false;
    if (!bind(pszPath)) {
        close();
        return false;
    }

    int nRet = SOCKET_SND_RCV_BUFFER;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &nRet, sizeof(nRet));
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &nRet, sizeof(nRet));

    int nRetLen = sizeof(unsigned int);
    if (getsockopt(
            socket_fd_, SOL_SOCKET, SO_SNDBUF, &nRet, reinterpret_cast<socklen_t*>(&nRetLen)) == 0)
        logger_->log(Logger::Info, "UUDP-Max Send Buffer=%d", nRet);
    nRetLen = sizeof(unsigned int);
    if (getsockopt(
            socket_fd_, SOL_SOCKET, SO_RCVBUF, &nRet, reinterpret_cast<socklen_t*>(&nRetLen)) == 0)
        logger_->log(Logger::Info, "UUDP-Max Recv Buffer=%d", nRet);

    logger_->log(Logger::Info, "[Socket:%s] opened!!", pszPath);
    return true;
}

bool Socket::open(UINT /*nPort*/, const char* /*pszAddr*/)
{
    return true;
}

void Socket::close()
{
    // Atomically swap the fd out so concurrent select()/recv() in the
    // owning thread observes INVALID_SOCKET on its next snapshot.
    const int fd = socket_fd_.exchange(INVALID_SOCKET, std::memory_order_acq_rel);
    if (fd != INVALID_SOCKET) {
        struct linger lg {
            1, 0
        };
        setsockopt(fd, SOL_SOCKET, SO_LINGER, static_cast<void*>(&lg), sizeof(lg));

        // ::close(fd) alone does NOT wake another thread parked in
        // recv/recvfrom on Linux — the kernel decrements the refcount but
        // the syscall keeps blocking. shutdown() actually unblocks pending
        // I/O on the peer's socket end, returning 0/EOF to the reader.
        // SHUT_RDWR covers both TCP recv() and UDP recvfrom(); errors are
        // ignored for sockets that don't support shutdown (e.g. unconnected
        // UNIX datagram before bind).
        ::shutdown(fd, SHUT_RDWR);

        if (::close(fd) == SOCKET_ERROR)
            logger_->log(Logger::Info, "close error: %s", strerror(errno));
    }
    if (!m_path.empty()) {
        unlink(m_path.c_str());
        m_path.clear();
    }
}

bool Socket::assign_socket(int hSocket, int nAddrType)
{
    socket_fd_ = hSocket;
    addr_type_ = nAddrType;
    return true;
}

bool Socket::create_socket(int nAddrFormat, int nSocketType, int nProtocolType)
{
    if (socket_fd_ != INVALID_SOCKET) {
        logger_->log(Logger::Info, "create_socket: already created");
        return false;
    }
    addr_type_ = nAddrFormat;
    socket_fd_ = socket(nAddrFormat, nSocketType, nProtocolType);
    if (socket_fd_ == INVALID_SOCKET) {
        logger_->log(Logger::Info, "socket error:%d:%s", errno, strerror(errno));
        return false;
    }
    if (nAddrFormat == AF_INET) {
        int opt = 1;
        if (setsockopt(
                socket_fd_, SOL_SOCKET, SO_REUSEADDR, static_cast<void*>(&opt), sizeof(opt)) != 0) {
            logger_->log(Logger::Info, "setsockopt(SO_REUSEADDR) failed");
            ::close(socket_fd_);
            socket_fd_ = INVALID_SOCKET;
            return false;
        }
    }
    return true;
}

bool Socket::bind(const char* pszPath)
{
    struct sockaddr_un sockUnix {};
    sockUnix.sun_family = static_cast<sa_family_t>(addr_type_);
    strncpy(sockUnix.sun_path, pszPath, sizeof(sockUnix.sun_path) - 1);
    UINT addrlen = sizeof(sockUnix.sun_family) + strlen(sockUnix.sun_path) + 1;
    unlink(pszPath);

    int ret = ::bind(socket_fd_, reinterpret_cast<SOCKADDR*>(&sockUnix), addrlen);
    logger_->log(Logger::Info, "Socket Bind [Ret:%d] path=%s", ret, pszPath);
    if (ret != SOCKET_ERROR)
        return true;
    logger_->log(Logger::Info, "Bind Error(%d,%s)", errno, strerror(errno));
    return false;
}

bool Socket::bind(UINT nPort, const char* pszAddr)
{
    SOCKADDR_IN sockAddr{};
    sockAddr.sin_family = static_cast<sa_family_t>(addr_type_);
    if (!pszAddr) {
        sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        long lResult = inet_addr(pszAddr);
        if (lResult == -1) {
            logger_->log(Logger::Info, "inet_addr(%s) failed", pszAddr);
            return false;
        }
        sockAddr.sin_addr.s_addr = static_cast<in_addr_t>(lResult);
    }
    sockAddr.sin_port = htons(static_cast<u_short>(nPort));

    int ret = ::bind(socket_fd_, reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr));
    logger_->log(Logger::Info,
                 "Socket Bind [Ret:%d] port=%d addr=%s",
                 ret,
                 ntohs(sockAddr.sin_port),
                 inet_ntoa(sockAddr.sin_addr));
    if (ret != SOCKET_ERROR) {
        int nRet = SOCKET_SND_RCV_BUFFER;
        int nRetLen = sizeof(unsigned int);
        setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &nRet, sizeof(nRet));
        setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &nRet, sizeof(nRet));
        if (getsockopt(
                socket_fd_, SOL_SOCKET, SO_SNDBUF, &nRet, reinterpret_cast<socklen_t*>(&nRetLen)) ==
            0)
            logger_->log(Logger::Info, "Socket-Max Send Buffer=%d", nRet);
        nRetLen = sizeof(unsigned int);
        if (getsockopt(
                socket_fd_, SOL_SOCKET, SO_RCVBUF, &nRet, reinterpret_cast<socklen_t*>(&nRetLen)) ==
            0)
            logger_->log(Logger::Info, "Socket-Max Recv Buffer=%d", nRet);
        return true;
    }
    logger_->log(Logger::Info, "Bind Error(%d,%s)", errno, strerror(errno));
    return false;
}

int Socket::select(int timeout)
{
    // Take ONE atomic snapshot at the top so a concurrent close() that
    // invalidates socket_fd_ between the FD_SET and the ::select call
    // cannot make us pass a stale fd. Subsequent operations all use the
    // local snapshot; if close() runs mid-call, ::select returns EBADF
    // which the caller treats as "exit cleanly".
    const int fd = socket_fd_.load(std::memory_order_acquire);
    if (fd == INVALID_SOCKET)
        return -1;
    fd_set fd_read;
    timeval sel_timeout;
    FD_ZERO(&fd_read);
    FD_SET(fd, &fd_read);
    sel_timeout.tv_sec = timeout / 1000;
    sel_timeout.tv_usec = (timeout % 1000) * 1000;
    int selnum = ::select(fd + 1, &fd_read, nullptr, nullptr, &sel_timeout);
    if (selnum == -1)
        return -1;
    if (selnum == 0 || !FD_ISSET(fd, &fd_read))
        return 0;
    return 1;
}

int Socket::select()
{
    const int fd = socket_fd_.load(std::memory_order_acquire);
    if (fd == INVALID_SOCKET)
        return -1;
    fd_set fd_read;
    FD_ZERO(&fd_read);
    FD_SET(fd, &fd_read);
    int selnum = ::select(fd + 1, &fd_read, nullptr, nullptr, nullptr);
    if (selnum == -1)
        return -1;
    if (selnum == 0 || !FD_ISSET(fd, &fd_read))
        return 0;
    return 1;
}

int Socket::recv(void* lpBuf, int nBufLen, int nFlags)
{
    return static_cast<int>(::recv(socket_fd_, static_cast<char*>(lpBuf), nBufLen, nFlags));
}

int Socket::recv_from(void* lpBuf, int nBufLen, char* rSocketAddress, UINT& rSocketPort, int nFlags)
{
    SOCKADDR sockAddr{};
    int nSockAddrLen = sizeof(sockAddr);
    int nRealRecvLen = static_cast<int>(recvfrom(socket_fd_,
                                                 static_cast<char*>(lpBuf),
                                                 nBufLen,
                                                 nFlags,
                                                 &sockAddr,
                                                 reinterpret_cast<socklen_t*>(&nSockAddrLen)));

    if (nRealRecvLen != SOCKET_ERROR) {
        if (addr_type_ == AF_UNIX) {
            auto* pUnixAddr = reinterpret_cast<struct sockaddr_un*>(&sockAddr);
            snprintf(rSocketAddress, 108, "%s", pUnixAddr->sun_path);
            logger_->log(Logger::Info, "[Socket] recv_from: addr=%s", rSocketAddress);
        } else {
            auto* pInetAddr = reinterpret_cast<struct sockaddr_in*>(&sockAddr);
            rSocketPort = ntohs(pInetAddr->sin_port);
            snprintf(rSocketAddress, 64, "%s", inet_ntoa(pInetAddr->sin_addr));
        }
    } else {
        logger_->log(Logger::Info, "[Socket] Recv Error=%d", errno);
    }
    return nRealRecvLen;
}

int Socket::send(const void* lpBuf, int nBufLen, int nFlags)
{
    return static_cast<int>(::send(socket_fd_, static_cast<const char*>(lpBuf), nBufLen, nFlags));
}

int Socket::send_to(const void* lpBuf,
                    int nBufLen,
                    UINT nHostPort,
                    const char* lpszHostAddress,
                    int nFlags,
                    bool /*bDebug*/)
{
    if (addr_type_ == AF_UNIX) {
        if (!lpszHostAddress)
            return 0;
        sockaddr_un to{};
        to.sun_family = AF_UNIX;
        strncpy(to.sun_path, lpszHostAddress, sizeof(to.sun_path) - 1);
        int nAddrLen = static_cast<int>(sizeof(to.sun_family) + strlen(to.sun_path));

        int len = 0, retry = 0;
        do {
            len = static_cast<int>(sendto(socket_fd_,
                                          static_cast<const char*>(lpBuf),
                                          nBufLen,
                                          nFlags,
                                          reinterpret_cast<SOCKADDR*>(&to),
                                          nAddrLen));
            if (len == nBufLen)
                return len;
            retry++;
            char cwd[512] = {};
            if (!getcwd(cwd, sizeof(cwd)))
                cwd[0] = '\0';
            logger_->log(__FILE__,
                         __LINE__,
                         Logger::Error,
                         "[AF_UNIX-send_to:%s] retry=%d send(%s:%d) error(%s) cwd=%s",
                         m_path.empty() ? "(null)" : m_path.c_str(),
                         retry,
                         lpszHostAddress,
                         len,
                         strerror(errno),
                         cwd);
            if (retry >= RETRY_COUNT)
                break;
            milli_sleep(WAIT_TIMEOUT);
        } while (!m_path.empty());
        return 0;
    }

    // AF_INET
    SOCKADDR sockAddr{};
    auto* pInetAddr = reinterpret_cast<struct sockaddr_in*>(&sockAddr);
    if (socket_fd_ == INVALID_SOCKET)
        return -1;
    pInetAddr->sin_family = AF_INET;

    if (!lpszHostAddress) {
        pInetAddr->sin_addr.s_addr = htonl(INADDR_BROADCAST);
    } else {
        pInetAddr->sin_addr.s_addr = inet_addr(lpszHostAddress);
        if (pInetAddr->sin_addr.s_addr == static_cast<in_addr_t>(-1)) {
            struct hostent* lphost = gethostbyname(lpszHostAddress);
            if (!lphost) {
                logger_->log(Logger::Info, "gethostbyname(%s) failed", lpszHostAddress);
                return SOCKET_ERROR;
            }
            pInetAddr->sin_addr.s_addr = reinterpret_cast<struct in_addr*>(lphost->h_addr)->s_addr;
        }
    }
    pInetAddr->sin_port = htons(static_cast<u_short>(nHostPort));
    return static_cast<int>(sendto(
        socket_fd_, static_cast<const char*>(lpBuf), nBufLen, nFlags, &sockAddr, sizeof(sockAddr)));
}

bool Socket::sock_name(char* rSocketAddress, UINT& rSocketPort)
{
    SOCKADDR sockAddr{};
    int nLen = sizeof(sockAddr);
    if (getsockname(socket_fd_, &sockAddr, reinterpret_cast<socklen_t*>(&nLen)) == SOCKET_ERROR) {
        logger_->log(Logger::Info, "getsockname error: %s", strerror(errno));
        return false;
    }
    if (addr_type_ == AF_UNIX) {
        snprintf(
            rSocketAddress, 108, "%s", reinterpret_cast<struct sockaddr_un*>(&sockAddr)->sun_path);
    } else {
        auto* p = reinterpret_cast<struct sockaddr_in*>(&sockAddr);
        rSocketPort = ntohs(p->sin_port);
        snprintf(rSocketAddress, 64, "%s", inet_ntoa(p->sin_addr));
    }
    return true;
}

bool Socket::peer_name(char* rPeerAddress, UINT& rPeerPort)
{
    SOCKADDR sockAddr{};
    int nLen = sizeof(sockAddr);
    if (getpeername(socket_fd_, &sockAddr, reinterpret_cast<socklen_t*>(&nLen)) == SOCKET_ERROR) {
        logger_->log(Logger::Info, "getpeername error: %s", strerror(errno));
        return false;
    }
    if (addr_type_ == AF_UNIX) {
        snprintf(
            rPeerAddress, 108, "%s", reinterpret_cast<struct sockaddr_un*>(&sockAddr)->sun_path);
    } else {
        auto* p = reinterpret_cast<struct sockaddr_in*>(&sockAddr);
        rPeerPort = ntohs(p->sin_port);
        snprintf(rPeerAddress, 64, "%s", inet_ntoa(p->sin_addr));
    }
    return true;
}

// ── TcpSocket ────────────────────────────────────────────────────────────────
TcpSocket::TcpSocket()
{
    memset(addr_buf_, 0, sizeof(addr_buf_));
}

bool TcpSocket::open(UINT nPort, const char* pszAddr)
{
    if (create_socket(AF_INET, SOCK_STREAM)) {
        if (bind(nPort, pszAddr))
            return true;
        logger_->log(__FILE__,
                     __LINE__,
                     Logger::Error,
                     "bind error(Port:%d,Err:%d-%s)",
                     nPort,
                     errno,
                     strerror(errno));
        close();
    }
    return false;
}

bool TcpSocket::open(const char* /*pszPath*/)
{
    return false;
}

void TcpSocket::close()
{
    Socket::close();
}

bool TcpSocket::prepare_to_client(UINT nPort, const char* pszAddr, bool bBlock, int nSec)
{
    port_ = nPort;
    strncpy(addr_buf_, pszAddr ? pszAddr : "", sizeof(addr_buf_) - 1);
    if (create_socket(AF_INET, SOCK_STREAM, 0)) {
        if (connect(nPort, pszAddr, bBlock, nSec)) {
            logger_->log(Logger::Info, "Connected...(%s,%d)", pszAddr, nPort);
            return true;
        }
        close();
        logger_->log(Logger::Info,
                     "Connect Fail...Addr:%s Err:%s",
                     pszAddr ? pszAddr : "null",
                     strerror(errno));
    }
    return false;
}

bool TcpSocket::prepare_to_server(UINT nPort, const char* pszAddr, int nConnectionBacklog)
{
    if (open(nPort, pszAddr)) {
        if (listen(nConnectionBacklog))
            return true;
    }
    return false;
}

bool TcpSocket::reconnect()
{
    int nCount = 0;
    bool bRet = false;
    while (!exit_) {
        close();
        milli_sleep(100);
        bRet = prepare_to_client(port_, addr_buf_, true, 5);
        if (bRet)
            break;
        nCount++;
        if (reconnect_retry_count_ != 0 && nCount > reconnect_retry_count_)
            break;
        logger_->log(
            Logger::Info, "reconnect: Retry=%d (wait:%d sec)", nCount, reconnect_interval_);
        Sleep(reconnect_interval_);
    }
    return bRet;
}

bool TcpSocket::listen(int nConnectionBacklog)
{
    if (::listen(socket_fd_, nConnectionBacklog) == SOCKET_ERROR) {
        logger_->log(__FILE__, __LINE__, Logger::Error,
                     "listen error(Err:%d-%s)", errno, strerror(errno));
        return false;
    }
    return true;
}

bool TcpSocket::connect(UINT nPort, const char* lpszAddr, bool bBlock, int nSec)
{
    if (!lpszAddr) {
        logger_->log(__FILE__, __LINE__, Logger::Error, "Tcp::connect(Addr==NULL)");
        return false;
    }
    SOCKADDR_IN sockAddr{};
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(lpszAddr);
    if (sockAddr.sin_addr.s_addr == INADDR_NONE) {
        struct hostent* lphost = gethostbyname(lpszAddr);
        if (lphost)
            sockAddr.sin_addr.s_addr = reinterpret_cast<struct in_addr*>(lphost->h_addr)->s_addr;
        else {
            logger_->log(__FILE__,
                         __LINE__,
                         Logger::Error,
                         "gethostbyname(%s) failed: %d-%s",
                         lpszAddr,
                         errno,
                         strerror(errno));
            return false;
        }
    }
    sockAddr.sin_port = htons(static_cast<unsigned short>(nPort));
    if (!bBlock)
        return connect(reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr));
    return connect(reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr), nSec);
}

bool TcpSocket::connect(const SOCKADDR* lpSockAddr, int nSockAddrLen)
{
    if (::connect(socket_fd_, lpSockAddr, nSockAddrLen) == SOCKET_ERROR) {
        logger_->log(__FILE__, __LINE__, Logger::Error,
                     "connect error(Err:%d-%s)", errno, strerror(errno));
        return false;
    }
    return true;
}

bool TcpSocket::connect(const SOCKADDR* lpSockAddr, int nSockAddrLen, int nSec)
{
    int nFlags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, nFlags | O_NONBLOCK);

    int nRet = ::connect(socket_fd_, lpSockAddr, nSockAddrLen);
    if (nRet == SOCKET_ERROR && errno != EINPROGRESS)
        return false;
    if (nRet != SOCKET_ERROR) {
        fcntl(socket_fd_, F_SETFL, nFlags);
        return true;
    }

    fd_set fd_r, fd_w;
    FD_ZERO(&fd_r);
    FD_SET(socket_fd_, &fd_r);
    fd_w = fd_r;
    timeval tv{nSec, 0};
    if (::select(socket_fd_ + 1, &fd_r, &fd_w, nullptr, &tv) == 0) {
        close();
        errno = ETIMEDOUT;
        return false;
    }
    if (FD_ISSET(socket_fd_, &fd_r) || FD_ISSET(socket_fd_, &fd_w)) {
        int nError = 0, nLen = sizeof(nError);
        if (getsockopt(
                socket_fd_, SOL_SOCKET, SO_ERROR, &nError, reinterpret_cast<socklen_t*>(&nLen)) ==
            SOCKET_ERROR)
            return false;
        fcntl(socket_fd_, F_SETFL, nFlags);
        if (nError) {
            close();
            errno = nError;
            return false;
        }
        return true;
    }
    logger_->log(Logger::Info, "Connect fail: fd_read, fd_write");
    return false;
}

bool TcpSocket::accept(SOCKADDR* lpSockAddr, int* lpSockAddrLen)
{
    if (socket_fd_ == INVALID_SOCKET) {
        logger_->log(Logger::Info, "Accept: Invalid");
        return false;
    }
    PDKSOCKET tmp = ::accept(socket_fd_, lpSockAddr, reinterpret_cast<socklen_t*>(lpSockAddrLen));
    if (tmp == INVALID_SOCKET) {
        logger_->log(__FILE__, __LINE__, Logger::Error, "Accept Err:%d,%s", errno, strerror(errno));
        return false;
    }
    logger_->log(Logger::Info, "Accept: Socket ID=%d", tmp);
    socket_fd_ = tmp;
    auto* pInetAddr = reinterpret_cast<struct sockaddr_in*>(lpSockAddr);
    logger_->log(Logger::Info,
                 "Accept from:%s port:%d",
                 inet_ntoa(pInetAddr->sin_addr),
                 ntohs(pInetAddr->sin_port));
    return true;
}

Socket* TcpSocket::accept()
{
    SOCKADDR_IN tSockAddr{};
    int nSockAddrLen = sizeof(SOCKADDR_IN);
    PDKSOCKET nSocket = ::accept(socket_fd_,
                                 reinterpret_cast<sockaddr*>(&tSockAddr),
                                 reinterpret_cast<socklen_t*>(&nSockAddrLen));
    if (nSocket == INVALID_SOCKET) {
        logger_->log(__FILE__, __LINE__, Logger::Error, "Accept Err:%d,%s", errno, strerror(errno));
        return nullptr;
    }
    Socket* pSocket = new Socket;
    pSocket->assign_socket(nSocket, AF_INET);
    return pSocket;
}

// ── UnixUdp ──────────────────────────────────────────────────────────────────
bool UnixUdp::open(const char* pszPath)
{
    return Socket::open(pszPath);
}
bool UnixUdp::open(UINT, const char*)
{
    return false;
}

int UnixUdp::send_to(const char* lpBuf, int nBuflen, const char* pszTo, bool bDebug)
{
    return Socket::send_to(lpBuf, nBuflen, 0, pszTo, 0, bDebug);
}

int UnixUdp::send_to(sockaddr_un* pTo, const char* lpBuf, int nBuflen, bool /*bDebug*/)
{
    int tolen = static_cast<int>(sizeof(pTo->sun_family) + strlen(pTo->sun_path));
    int len = 0, retry = 0;
    do {
        len = static_cast<int>(
            sendto(socket_fd_, lpBuf, nBuflen, 0, reinterpret_cast<sockaddr*>(pTo), tolen));
        if (len == nBuflen)
            return 1;
        retry++;
        if (retry >= RETRY_COUNT)
            break;
        milli_sleep(WAIT_TIMEOUT);
    } while (!m_path.empty());
    return 0;
}

int UnixUdp::recv_from(char* pBuf, int buflen, sockaddr_un* pFrom)
{
    int fromlen = sizeof(sockaddr_un);
    int len = 0, retry = 0;
    do {
        len = static_cast<int>(pFrom ? recvfrom(socket_fd_,
                                                pBuf,
                                                buflen,
                                                0,
                                                reinterpret_cast<sockaddr*>(pFrom),
                                                reinterpret_cast<socklen_t*>(&fromlen))
                                     : recvfrom(socket_fd_, pBuf, buflen, 0, nullptr, nullptr));
        if (len > 0)
            return len;
        retry++;
        if (retry >= RETRY_COUNT)
            break;
        milli_sleep(WAIT_TIMEOUT);
    } while (!m_path.empty());
    return 0;
}

// ── InetUdpSocket ────────────────────────────────────────────────────────────
bool InetUdpSocket::open(const char* /*pszPath*/)
{
    return false;
}

bool InetUdpSocket::open(UINT nPort, const char* pszAddr)
{
    if (create_socket(AF_INET, SOCK_DGRAM, 0)) {
        if (bind(nPort, pszAddr))
            return true;
        logger_->log(Logger::Info, "port:%d error:%d:%s", nPort, errno, strerror(errno));
        close();
    }
    return false;
}

}  // namespace pdk
