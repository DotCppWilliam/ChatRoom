#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <netinet/in.h>

namespace net 
{
    class Addr 
    {
    public:
        Addr(const std::string host, unsigned short port);
        Addr(unsigned short port = 0) : Addr("", port) {}
        Addr(const struct sockaddr_in& addr) : addr_(addr) {}
    public:
        std::string ToString() const;
        std::string Ip() const;
        unsigned short Port() const;
        unsigned int IpToInt() const;
        bool IsValid() const;
        struct sockaddr_in& GetAddr()
        { return addr_; }

        static std::string HostToIp(const std::string& host)
        {
            Addr addr(host, 0);
            return addr.Ip();
        }
    private:
        struct sockaddr_in addr_;
    };
}