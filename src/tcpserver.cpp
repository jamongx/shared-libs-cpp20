// tcpserver.cpp – TCP server implementation (C++17)
#include <cerrno>
#include <cstring>
#include "eventdefs.hpp"
#include "tcpserver.hpp"
#include "stream.hpp"

namespace pdk {

// ── TcpChannel ───────────────────────────────────────────────────────────────
TcpChannel::TcpChannel(TcpServer* server, Socket* sock)
    : server_(server),
      sock_(sock),
      logger_(Logger::get())
{}

TcpChannel::~TcpChannel()
{
    if (server_ && server_->OnClosed)
        server_->OnClosed(this);
    delete sock_;
    logger_->log(Logger::Info, "~TcpChannel");
}

int TcpChannel::send(void* Buffer, int nLen)
{
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
        if (!sock_)
            break;
        nLen = sock_->recv(Buffer, nPkSize);
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
                delete sock_;
                sock_ = nullptr;
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
TcpServer::TcpServer() : socket_(new TcpSocket), logger_(Logger::get()) {}

TcpServer::~TcpServer()
{
    OnAfterReceive = nullptr;
    OnBeforeSend = nullptr;
    OnConnected = nullptr;

    {
        AutoLock lk(&lock_);
        for (auto* ch : channels_)
            delete ch;
        channels_.clear();
    }
    delete socket_;
    logger_->log(Logger::Info, "~TcpServer End");
}

void TcpServer::close_channel(TcpChannel* channel)
{
    logger_->log(Logger::Info, "Channel[%p] Closed", static_cast<void*>(channel));
    AutoLock lk(&lock_);
    auto it = std::find(channels_.begin(), channels_.end(), channel);
    if (it != channels_.end()) {
        channels_.erase(it);
        delete channel;
    }
}

void TcpServer::check_channels()
{
    AutoLock lk(&lock_);
    for (auto it = channels_.begin(); it != channels_.end();) {
        TcpChannel* ch = *it;
        if (ch->status() & TCP_DISC) {
            it = channels_.erase(it);
            delete ch;
        } else {
            ++it;
        }
    }
}

void TcpServer::send_all(void* Buffer, int nLen)
{
    AutoLock lk(&lock_);
    for (auto* ch : channels_) {
        if (OnBeforeSend)
            OnBeforeSend(ch, Buffer, &nLen);
    }
}

void TcpServer::broadcast(void* Buffer, int nLen)
{
    AutoLock lk(&lock_);
    PDK16U chType = EVENTINFO_TYPE(reinterpret_cast<PEVENTINFO>(Buffer));
    for (auto* ch : channels_) {
        if ((ch->status() & TCP_CONN) && ch->type() == chType) {
            if (OnBeforeSend)
                OnBeforeSend(ch, Buffer, &nLen);
        }
    }
}

void TcpServer::start(const char* pszAddr, int32_t nPort)
{
    int nRetryCount = 0;
    if (pszAddr)
        addr_ = pszAddr;
    port_ = nPort;

    auto tryBind = [&](int port) -> bool {
        if (!addr_.empty())
            return socket_->prepare_to_server(port, addr_.c_str());
        return socket_->prepare_to_server(port, nullptr);
    };

    while (!tryBind(port_)) {
        logger_->log(Logger::Error, "Server Socket Error Addr:%s Port:%d", addr_.c_str(), port_);
        port_++;
        if (nRetryCount++ >= 2)
            break;
    }

    logger_->log(Logger::Info, "TcpServer Addr:%s Port:%d", addr_.c_str(), port_);
    create();
}

void* TcpServer::thread_proc()
{
    int nResult = 0;
    while (!do_exit()) {
        nResult = socket_->select();
        if (nResult == -1) {
            logger_->log("TcpServer #%d, %s", errno, strerror(errno));
            break;
        }
        check_channels();
        if (nResult == 0)
            continue;

        Socket* client = nullptr;
        try {
            client = socket_->accept();
        } catch (...) {
            logger_->log(Logger::Info, "TcpServer Accept Error");
            break;
        }
        if (!client)
            continue;

        {
            AutoLock lk(&lock_);
            logger_->log(Logger::Info, "Connected Count[%d:%zu]", limit_, channels_.size());
            if (static_cast<int>(channels_.size()) == limit_) {
                logger_->log(Logger::Error, "Full MAX[%d] connections", limit_);
                delete client;
                continue;
            }
        }

        auto* ch = new TcpChannel(this, client);
        if (OnConnected)
            OnConnected(ch);
        if (!ch->create()) {
            logger_->log(Logger::Error, "Channel Creation Error");
            delete ch;
            continue;
        }
        logger_->log(Logger::Info, "New Channel[%p]", static_cast<void*>(ch));

        AutoLock lk(&lock_);
        channels_.push_back(ch);
    }
    return nullptr;
}

}  // namespace pdk
