// udpserver.cpp – UDP server implementations (C++17)
#include <cerrno>
#include <cstring>
#include "udpserver.hpp"
#include "util.hpp"

namespace pdk {

// ── UUdpSender ───────────────────────────────────────────────────────────────
UUdpSender::UUdpSender() : logger_(Logger::get()) {}

bool UUdpSender::open(QThread* pOwner)
{
    owner_ = pOwner;
    return create_socket(AF_UNIX, SOCK_DGRAM, 0);
}

int UUdpSender::send_to(PEVENTINFO pInfo, const char* pszDest)
{
    return send_to(static_cast<void*>(pInfo), EVENTINFO_LENGTH(pInfo), pszDest);
}

int UUdpSender::send_to(void* pInfo, int nLen, const char* pszDest)
{
    return UnixUdp::send_to(static_cast<const char*>(pInfo), nLen, pszDest);
}

// ── UUdpReceiver ─────────────────────────────────────────────────────────────
UUdpReceiver::UUdpReceiver() : udp_recv_sock_(std::make_unique<UnixUdp>()), logger_(Logger::get())
{}

UUdpReceiver::~UUdpReceiver()
{
    OnAfterReceive = nullptr;
}

void UUdpReceiver::add_event(PEVENTINFO pBuffer)
{
    int nLen = EVENTINFO_LENGTH(pBuffer);
    if (OnAfterReceive)
        OnAfterReceive(owner_, pBuffer, &nLen);
}

void UUdpReceiver::add_event(void* pBuffer, int nLen)
{
    EVENTINFO Event{};
    EVENTINFO_TYPE(&Event) = 1;
    EVENTINFO_SUBTYPE(&Event) = 2;
    if (pBuffer && nLen > 0)
        memcpy(Event.Value, pBuffer, nLen);
    EVENTINFO_LENGTH(&Event) = static_cast<int>(EVENTINFO_HEADER_LENGTH) + nLen;
    nLen = EVENTINFO_LENGTH(&Event);
    if (OnAfterReceive)
        OnAfterReceive(owner_, &Event, &nLen);
}

void UUdpReceiver::start(QThread* pOwner, const char* pszPath)
{
    owner_ = pOwner;
    if (!udp_recv_sock_->open(pszPath))
        throw Exception::format(
            "UUdpReceiver[%s] Create Error[%d:%s]", pszPath, errno, strerror(errno));
    create();
}

void* UUdpReceiver::thread_proc()
{
    EVENTINFO Packet{};
    PEVENTINFO pPacket = &Packet;
    int nPkSize = sizeof(EVENTINFO);
    sockaddr_un from{};
    int nLen = 0;

    logger_->log(Logger::Info, "UUdpReceiver Start thread_proc");
    while (!do_exit()) {
        nLen = udp_recv_sock_->recv_from(reinterpret_cast<char*>(pPacket), nPkSize, &from);
        if (nLen < 1) {
            if (errno != 0)
                logger_->log(Logger::Error, "UUdpReceiver Err:%d:%s", errno, strerror(errno));
            break;
        }
        if (nLen != pPacket->h.nMsgLen) {
            logger_->log(
                Logger::Error, "UUdpReceiver Packet Err:%d Recv:%d", pPacket->h.nMsgLen, nLen);
            pPacket->h.nMsgLen = nLen;
        }
        add_event(pPacket);
    }
    return nullptr;
}

// ── UUdpServer ───────────────────────────────────────────────────────────────
UUdpServer::UUdpServer() : udp_send_sock_(std::make_unique<UUdpSender>()) {}

void UUdpServer::start(QThread* pOwner, const char* pszPath)
{
    owner_ = pOwner;
    if (!udp_send_sock_->open(pOwner))
        logger_->log(Logger::Error, "UUdpServer: failed to open send socket");
    UUdpReceiver::start(pOwner, pszPath);
}

int UUdpServer::send_to(PEVENTINFO pInfo, const char* pszDest)
{
    int nLen = EVENTINFO_LENGTH(pInfo);
    if (OnBeforeSend)
        OnBeforeSend(owner_, pInfo, &nLen);
    return udp_send_sock_->send_to(pInfo, pszDest);
}

int UUdpServer::send_to(void* pInfo, int nLen, const char* pszDest)
{
    if (OnBeforeSend)
        OnBeforeSend(owner_, pInfo, &nLen);
    return udp_send_sock_->send_to(pInfo, nLen, pszDest);
}

// ── IUdpSender ───────────────────────────────────────────────────────────────
IUdpSender::IUdpSender() : logger_(Logger::get()) {}

bool IUdpSender::open(QThread* pOwner)
{
    owner_ = pOwner;
    return InetUdpSocket::open(0, nullptr);
}

int IUdpSender::send_to(void* pBuffer, int nLen, const char* pszAddr, int nPort)
{
    return InetUdpSocket::send_to(static_cast<const char*>(pBuffer), nLen, nPort, pszAddr);
}

int IUdpSender::send_to(PEVENTINFO pInfo, const char* pszAddr, int nPort)
{
    return InetUdpSocket::send_to(static_cast<const char*>(static_cast<void*>(pInfo)),
                                  EVENTINFO_LENGTH(pInfo),
                                  nPort,
                                  pszAddr);
}

// ── IUdpReceiver ─────────────────────────────────────────────────────────────
IUdpReceiver::IUdpReceiver()
    : udp_recv_sock_(std::make_unique<InetUdpSocket>()),
      logger_(Logger::get())
{}

IUdpReceiver::~IUdpReceiver()
{
    OnAfterReceive = nullptr;
}

void IUdpReceiver::add_event(void* Buffer)
{
    int nLen = EVENTINFO_LENGTH(static_cast<PEVENTINFO>(Buffer));
    if (OnAfterReceive)
        OnAfterReceive(owner_, Buffer, &nLen);
}

void IUdpReceiver::add_event(void* Buffer, int nLen)
{
    if (OnAfterReceive)
        OnAfterReceive(owner_, Buffer, &nLen);
}

bool IUdpReceiver::start(QThread* pOwner, const char* pszAddr, int32_t nPort)
{
    owner_ = pOwner;
    addr_ = pszAddr ? pszAddr : "";
    port_ = nPort;
    if (!udp_recv_sock_->open(nPort, pszAddr))
        throw Exception::format("IUdpReceiver[%s:%d] Create Error[%d:%s]",
                                addr_.c_str(),
                                nPort,
                                errno,
                                strerror(errno));
    return create() == 1;
}

void* IUdpReceiver::thread_proc()
{
    UINT port = 0;
    char addr[1024]{};
    EVENTINFO Packet{};
    PEVENTINFO pPacket = &Packet;
    int nPkSize = sizeof(EVENTINFO);
    int nLen = 0;

    while (!do_exit()) {
        nLen = udp_recv_sock_->recv_from(reinterpret_cast<char*>(pPacket), nPkSize, addr, port);
        if (nLen < 1) {
            if (errno != 0)
                logger_->log("Recv Err:%d:%s", errno, strerror(errno));
            break;
        }
        if (nLen != pPacket->h.nMsgLen) {
            logger_->log("Packet Err:%d Recv:%d", pPacket->h.nMsgLen, nLen);
            pPacket->h.nMsgLen = nLen;
        }
        add_event(pPacket);
    }
    return nullptr;
}

// ── IUdpServer ───────────────────────────────────────────────────────────────
IUdpServer::IUdpServer() : udp_send_sock_(std::make_unique<IUdpSender>()) {}

bool IUdpServer::start(QThread* pOwner, const char* pszAddr, int32_t nPort)
{
    if (!udp_send_sock_->open(pOwner))
        throw Exception::format("IUdpServer[0:NULL] Create Error[%d:%s]", errno, strerror(errno));
    return IUdpReceiver::start(pOwner, pszAddr, nPort);
}

int IUdpServer::send_to(void* pBuffer, int nLen, const char* pszAddr, int nPort)
{
    if (OnBeforeSend)
        OnBeforeSend(owner_, pBuffer, &nLen);
    return udp_send_sock_->send_to(pBuffer, nLen, pszAddr, nPort);
}

int IUdpServer::send_to(PEVENTINFO pInfo, const char* pszAddr, int32_t nPort)
{
    int nLen = EVENTINFO_LENGTH(pInfo);
    if (OnBeforeSend)
        OnBeforeSend(owner_, pInfo, &nLen);
    return udp_send_sock_->send_to(pInfo, pszAddr, nPort);
}

// ── UUdpIfThread ─────────────────────────────────────────────────────────────
UUdpIfThread::UUdpIfThread()
    : QThread(DEF_EVENT_QUEUE_LENGTH),
      udp_channel_(std::make_unique<UUdpServer>()),
      logger_(Logger::get())
{
    udp_channel_->OnAfterReceive = after_receive;
    udp_channel_->OnBeforeSend = before_send;
}

UUdpIfThread::UUdpIfThread(int n)
    : QThread(n),
      udp_channel_(std::make_unique<UUdpServer>()),
      logger_(Logger::get())
{
    udp_channel_->OnAfterReceive = after_receive;
    udp_channel_->OnBeforeSend = before_send;
}

void UUdpIfThread::start(const char* pszPath)
{
    path_ = pszPath;
    udp_channel_->start(this, pszPath);
    create();
}

int32_t UUdpIfThread::send_to(const char* pszPath, void* Buffer, int32_t& nLen)
{
    send_handle(Buffer, nLen);
    EVENTINFO_LENGTH(reinterpret_cast<PEVENTINFO>(Buffer)) = nLen;
    return udp_channel_->send_to(reinterpret_cast<PEVENTINFO>(Buffer), pszPath);
}

void* UUdpIfThread::thread_proc()
{
    while (!do_exit()) {
        PEVENTINFO pPacket = remove();
        recv_handle(pPacket, EVENTINFO_LENGTH(pPacket));
    }
    return nullptr;
}

}  // namespace pdk
