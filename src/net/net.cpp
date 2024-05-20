#include "net.h"

#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <syscall.h>
#include <netinet/tcp.h>

namespace net 
{
    struct in_addr GetHostByname(const std::string& host)
    {
        struct in_addr addr;
        char buf[1024];
        struct hostent hent;
        struct hostent* he = nullptr;
        int err_no = 0;
        memset(&hent, 0, sizeof(hent));
        int ret = gethostbyname_r(host.c_str(), &hent, buf, sizeof(buf), &he, &err_no);
        if (ret == 0 && he && he->h_addrtype == AF_INET)
            addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
        else
            addr.s_addr = INADDR_NONE;

        return addr;
    }


    uint64_t Gettid()
    { return syscall(SYS_gettid); }



    int SetNonBlock(int fd, bool value)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0)
            return errno;
        if (value)
            return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    int SetReuseAddr(int fd, bool value)
    {
        int flag = value;
        int len = sizeof flag;
        return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, len);
    }

    int SetReusePort(int fd, bool value)
    {
        int flag = value;
        int len = sizeof flag;
        return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &flag, len);
    }

    int SetNoDelay(int fd, bool value)
    {
        int flag = value;
        int len = sizeof flag;
        return setsockopt(fd, SOL_SOCKET, TCP_NODELAY, &flag, len);
    }
}