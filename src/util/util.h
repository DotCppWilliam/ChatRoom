#pragma once

#include "noncopyable.h"
#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <functional>
#include <chrono>

namespace util
{
    std::string FormatIpPort(uint32_t ip, unsigned short port);
    std::string FormatIp(uint32_t ip);
    std::string Format(const char *fmt, ...);

    static int64_t TimeMicro()
    {
        std::chrono::time_point<std::chrono::system_clock> p = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(p.time_since_epoch()).count();
    }


    static int64_t TimeMilli() { return TimeMicro() / 1000; }
    int AddFdFlag(int fd, int flag);
    


    class ExitCaller : private util::NonCopyable 
    {
    public:
        ~ExitCaller() { functor_(); }
        ExitCaller(std::function<void()> &&functor) : functor_(std::move(functor)) {}
    private:
        std::function<void()> functor_;
    };
}
