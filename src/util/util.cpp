#include "util.h"


#include <fcntl.h>
#include <memory>
#include <stdarg.h>

namespace util 
{
    std::string Format(const char *fmt, ...) 
    {
        char buffer[500];
        std::unique_ptr<char[]> release1;
        char *base;
        for (int iter = 0; iter < 2; iter++) 
        {
            int bufsize;
            if (iter == 0) 
            {
                bufsize = sizeof(buffer);
                base = buffer;
            } 
            else 
            {
                bufsize = 30000;
                base = new char[bufsize];
                release1.reset(base);
            }
            char *p = base;
            char *limit = base + bufsize;
            if (p < limit) 
            {
                va_list ap;
                va_start(ap, fmt);
                p += vsnprintf(p, limit - p, fmt, ap);
                va_end(ap);
            }
            // Truncate to available space if necessary
            if (p >= limit) 
            {
                if (iter == 0) 
                {
                    continue;  // Try again with larger buffer
                } 
                else 
                {
                    p = limit - 1;
                    *p = '\0';
                }
            }
            break;
        }
        return base;
    }

    std::string FormatIpPort(uint32_t ip, unsigned short port)
    {
        return Format("%d.%d.%d.%d:%d", (ip >> 0) & 0xff, (ip >> 8) & 0xff, 
            (ip >> 16) & 0xff, (ip >> 24) & 0xff, ntohs(port));
    }
    
    std::string FormatIp(uint32_t ip)
    {
        return Format("%d.%d.%d.%d", (ip >> 0) & 0xff, (ip >> 8) & 0xff, 
            (ip >> 16) & 0xff, (ip >> 24) & 0xff);
    }

    int AddFdFlag(int fd, int flag) 
    {
        int ret = fcntl(fd, F_GETFD);
        return fcntl(fd, F_SETFD, ret | flag);
    }

}
