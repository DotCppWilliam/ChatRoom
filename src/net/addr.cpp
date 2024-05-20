#include "addr.h"
#include "log.h"
#include "net.h"

#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>

namespace net 
{
    Addr::Addr(const std::string host, unsigned short port)
    {
        memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        if (host.size())
            addr_.sin_addr = GetHostByname(host);
        else
            addr_.sin_addr.s_addr = INADDR_ANY;

        if (addr_.sin_addr.s_addr == INADDR_NONE)
            LOG_FMT_ERROR_MSG("无法解析ip: %s\n", host.c_str());
    }

    std::string Addr::ToString() const 
    {
        uint32_t ip = addr_.sin_addr.s_addr;
        return util::FormatIpPort(ip, ntohs(addr_.sin_port));
    }

    std::string Addr::Ip() const
    {
        uint32_t ip = addr_.sin_addr.s_addr;
        return util::FormatIp(ip);
    }

    unsigned short Addr::Port() const
    {
        return static_cast<unsigned short>(ntohs(addr_.sin_port));
    }

    unsigned int Addr::IpToInt() const
    {
        return ntohl(addr_.sin_addr.s_addr);
    }

    bool Addr::IsValid() const
    {
        return addr_.sin_addr.s_addr != INADDR_NONE;
    }


}