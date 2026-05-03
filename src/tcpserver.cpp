// tcpserver.cpp – TCP server implementation (C++17)
#include <algorithm>
#include <cerrno>
#include <cstring>
#include "eventdefs.hpp"
#include "tcpserver.hpp"
#include "stream.hpp"

namespace pdk {

namespace {
// Accept-loop poll period. Determines worst-case shutdown latency for the
// server thread when the listening socket has no pending connection.
constexpr int kAcceptSelectTimeoutMs = 100;
}  // namespace

// ── TcpChannel ───────────────────────────────────────────────────────────────
TcpChannel::TcpChannel(TcpServer* server, std::unique_ptr<Socket> sock)
    : server_(server),
      sock_(std::move(sock)),
      logger_(Logger::get())
{}

TcpChannel::~TcpChannel()
{
    // Order matters:
    //   1. Set stop flag and (best-effort) close the socket so a thread
    //      blocked in recv() wakes immediately with an error.
    //   2. Join the recv thread.
    //   3. Notify owner via OnClosed.
    //   4. Destroy the unique_ptr<Socket>.
    // Earlier code deleted sock_ before joining, allowing the recv loop to
    // touch a destroyed file descriptor.
    close_pre();
    if (sock_) sock_->close();
    close_post();
    if (server_ && server_->OnClosed)
        server_->OnClosed(this);
    logger_->log(Logger::Info, "~TcpChannel");
}

int TcpChannel::send(void* Buffer, int nLen)
{
    if (!sock_) return -1;
    int nCount = sock_->send(Buffer, nLen);
    if (nCount == -1) {
        logger_->log(Logger::Info, "TcpChannel [%d:%s]", errno, strerror(errno));
        close();
    }
    return nCount;
}

void* TcpChannel::thread_proc()
{
    int nLen = 0;
    uint8_t Buffer[1024 * 10]{};
    auto* pPacket = reinterpret_cast<PEVENTINFO>(Buffer);
    int nPkSize = sizeof(EVENTINFO);
    MemoryStream m_Stream;

    while (!do_exit()) {
        Socket* s = sock_.get();
        if (!s)
            break;
        nLen = s->recv(Buffer, nPkSize);
        if (nLen < 1) {
            if (errno != 0)
                logger_->log("Recv Err:%d:%s", errno, strerror(errno));
            break;
        }
        if (!server_->use_event()) {
            if (server_ && server_->OnAfterReceive)
                server_->OnAfterReceive(this, static_cast<void*>(Buffer), &nLen);
            continue;
        }
        m_Stream.write(static_cast<const void*>(Buffer), nLen);
        while (true) {
            pPacket = reinterpret_cast<PEVENTINFO>(m_Stream.data());
            if (!pPacket)
                break;

            int nPkLen = pPacket->h.nMsgLen;
            if (nPkLen < 1) {
                sock_.reset();   // close + delete via unique_ptr
                logger_->log(Logger::Error, "Illegal Packet Size[%d] => Dropped", nPkLen);
                break;
            }
            if (nPkLen > m_Stream.GetSize())
                break;

            int nSize = m_Stream.GetSize() - nPkLen;
            if (server_ && server_->OnAfterReceive)
                server_->OnAfterReceive(this, pPacket, &pPacket->h.nMsgLen);

            uint8_t* p = m_Stream.data();
            if (nSize > 0)
                memcpy(p, p + nPkLen, nSize);
            m_Stream.SetSize(nSize);
        }
    }
    status_ = TCP_DISC;
    return nullptr;
}

// ── TcpServer ────────────────────────────────────────────────────────────────
TcpServer::TcpServer() : socket_(std::make_unique<TcpSocket>()), logger_(Logger::get()) {}

TcpServer::~TcpServer()
{
    // First, stop the accept thread and any peer channels. Without this,
    // ~TcpServer would race with thread_proc still pushing into channels_
    // (or calling OnConnected back into the half-destroyed object) and
    // could touch the listening socket after we destroy it.
    close();

    OnAfterReceive = nullptr;
    OnBeforeSend   = nullptr;
    OnConnected    = nullptr;

    {
        std::scoped_lock lk(lock_);
        channels_.clear();   // each unique_ptr<TcpChannel> joins its recv thread
    }
    // socket_ unique_ptr handles the listening socket destruction.
    logger_->log(Logger::Info, "~TcpServer End");
}

void TcpServer::close()
{
    // Order matters: signal stop, close the listening socket so select()
    // wakes immediately, then join. Without the explicit socket close, the
    // accept thread could be parked in select() forever.
    close_pre();
    if (socket_)
        socket_->close();
    close_post();
}

void TcpServer::close_channel(TcpChannel* channel)
{
    logger_->log(Logger::Info, "Channel[%p] Closed", static_cast<void*>(channel));
    std::scoped_lock lk(lock_);
    auto it = std::find_if(channels_.begin(), channels_.end(),
                           [&](const std::unique_ptr<TcpChannel>& p) {
                               return p.get() == channel;
                           });
    if (it != channels_.end())
        channels_.erase(it);   // unique_ptr handles delete + signal
}

void TcpServer::check_channels()
{
    std::scoped_lock lk(lock_);
    for (auto it = channels_.begin(); it != channels_.end();) {
        if ((*it)->status() & TCP_DISC) {
            it = channels_.erase(it);
        } else {
            ++it;
        }
    }
}

void TcpServer::send_all(void* Buffer, int nLen)
{
    std::scoped_lock lk(lock_);
    for (auto& ch : channels_) {
        if (OnBeforeSend)
            OnBeforeSend(ch.get(), Buffer, &nLen);
    }
}

void TcpServer::broadcast(void* Buffer, int nLen)
{
    std::scoped_lock lk(lock_);
    PDK16U chType = EVENTINFO_TYPE(reinterpret_cast<PEVENTINFO>(Buffer));
    for (auto& ch : channels_) {
        if ((ch->status() & TCP_CONN) && ch->type() == chType) {
            if (OnBeforeSend)
                OnBeforeSend(ch.get(), Buffer, &nLen);
        }
    }
}

bool TcpServer::start(const char* pszAddr, int32_t nPort)
{
    if (pszAddr)
        addr_ = pszAddr;
    port_ = nPort;

    auto try_bind = [&](int port) -> bool {
        const char* addr = addr_.empty() ? nullptr : addr_.c_str();
        return socket_->prepare_to_server(port, addr);
    };

    if (!try_bind(port_)) {
        logger_->log(Logger::Error,
                     "TcpServer bind failed: addr=%s port=%d (caller must choose a free port)",
                     addr_.c_str(), port_);
        return false;
    }

    logger_->log(Logger::Info, "TcpServer Addr:%s Port:%d", addr_.c_str(), port_);
    return create() == 1;
}

void* TcpServer::thread_proc()
{
    while (!do_exit()) {
        // Timed select so close() is observed within kAcceptSelectTimeoutMs
        // even when no connection is pending.
        int nResult = socket_->select(kAcceptSelectTimeoutMs);
        if (nResult == -1) {
            // EBADF after socket close during shutdown is expected; only log
            // when we are not exiting.
            if (!do_exit())
                logger_->log("TcpServer #%d, %s", errno, strerror(errno));
            break;
        }
        check_channels();
        if (nResult == 0)
            continue;   // timeout, just loop and re-check do_exit

        std::unique_ptr<Socket> client;
        try {
            client.reset(socket_->accept());
        } catch (...) {
            if (!do_exit())
                logger_->log(Logger::Info, "TcpServer Accept Error");
            break;
        }
        if (!client)
            continue;

        {
            std::scoped_lock lk(lock_);
            logger_->log(Logger::Info, "Connected Count[%d:%zu]", limit_, channels_.size());
            if (static_cast<int>(channels_.size()) == limit_) {
                logger_->log(Logger::Error, "Full MAX[%d] connections", limit_);
                continue;   // unique_ptr drops the rejected client cleanly
            }
        }

        auto ch = std::make_unique<TcpChannel>(this, std::move(client));
        if (OnConnected)
            OnConnected(ch.get());
        if (!ch->create()) {
            logger_->log(Logger::Error, "Channel Creation Error");
            continue;   // unique_ptr drops the channel cleanly
        }
        logger_->log(Logger::Info, "New Channel[%p]", static_cast<void*>(ch.get()));

        std::scoped_lock lk(lock_);
        channels_.push_back(std::move(ch));
    }
    return nullptr;
}

}  // namespace pdk
