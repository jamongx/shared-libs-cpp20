// ifconfig.h – Network interface configuration (C++17, Linux)
#pragma once

#include <cerrno>
#include <cstring>
#include <list>
#include <string>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <unistd.h>
#include "base.hpp"

namespace pdk {

class IfConfig {
public:
    IfConfig();
    ~IfConfig();

    const std::list<std::string>& interfaces();
    short flags(const char* pszifname);
    std::string addr(const char* pszifname);
    std::string mask(const char* pszifname);
    std::string broadcast_addr(const char* pszifname);
    std::string mac_addr(const char* pszifname);
    bool is_up(const char* pszifname);

protected:
    void refresh_interfaces();
    std::string _getaddr(const char* pszifname, int request);

    int sock_fd_{-1};
    std::list<std::string> iface_list_;
};

inline IfConfig::IfConfig()
{
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ == -1)
        throw Exception::format("socket create error[%d:%s]", errno, strerror(errno));
    refresh_interfaces();
}

inline IfConfig::~IfConfig()
{
    if (sock_fd_ != -1)
        close(sock_fd_);
}

inline void IfConfig::refresh_interfaces()
{
    struct ifconf ifc;
    char buf[4096];
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if (ioctl(sock_fd_, SIOCGIFCONF, &ifc) < 0)
        throw Exception::format("ioctl SIOCGIFCONF [%d:%s]", errno, strerror(errno));

    iface_list_.clear();
    struct ifreq* ifrp = ifc.ifc_req;
    for (int n = static_cast<int>(ifc.ifc_len / sizeof(struct ifreq)); n > 0; n--, ifrp++)
        iface_list_.push_back(ifrp->ifr_name);
}

inline std::string IfConfig::_getaddr(const char* pszifname, int request)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, pszifname, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(sock_fd_, request, &ifr) < 0)
        throw Exception::format("ioctl [%d:%s]", errno, strerror(errno));

    auto* paddr = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
    return inet_ntoa(paddr->sin_addr);
}

inline const std::list<std::string>& IfConfig::interfaces()
{
    refresh_interfaces();
    return iface_list_;
}

inline short IfConfig::flags(const char* pszifname)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, pszifname, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(sock_fd_, SIOCGIFFLAGS, &ifr) < 0)
        throw Exception::format("ioctl SIOCGIFFLAGS [%d:%s]", errno, strerror(errno));
    return ifr.ifr_flags;
}

inline std::string IfConfig::addr(const char* pszifname)
{
    return _getaddr(pszifname, SIOCGIFADDR);
}

inline std::string IfConfig::mask(const char* pszifname)
{
    return _getaddr(pszifname, SIOCGIFNETMASK);
}

inline std::string IfConfig::broadcast_addr(const char* pszifname)
{
    return _getaddr(pszifname, SIOCGIFBRDADDR);
}

inline std::string IfConfig::mac_addr(const char* pszifname)
{
    std::string addr = addr(pszifname);
    struct arpreq arp {};
    auto* sin = reinterpret_cast<struct sockaddr_in*>(&arp.arp_pa);
    sin->sin_family = AF_INET;
    arp.arp_pa.sa_family = AF_INET;
    sin->sin_addr.s_addr = inet_addr(addr.c_str());

    if (sin->sin_addr.s_addr == static_cast<in_addr_t>(-1)) {
        struct hostent* hp = gethostbyname(addr.c_str());
        if (hp)
            memcpy(&sin->sin_addr, hp->h_addr, sizeof(sin->sin_addr));
    }
    if (ioctl(sock_fd_, SIOCGARP, &arp) < 0)
        throw Exception::format("ioctl SIOCGARP [%d:%s]", errno, strerror(errno));

    std::string mac;
    char hex[4];
    for (int i = 0; i < 6; i++) {
        snprintf(hex, sizeof(hex), "%02x", static_cast<unsigned char>(arp.arp_ha.sa_data[i]));
        if (!mac.empty())
            mac += ':';
        mac += hex;
    }
    return mac;
}

inline bool IfConfig::is_up(const char* pszifname)
{
    return (flags(pszifname) & IFF_UP) != 0;
}

}  // namespace pdk
