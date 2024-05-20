#pragma once

#include "noncopyable.h"
#include "channel.h"

#include <poll.h>
#include <sys/epoll.h>
#include <atomic>

namespace net
{
    const int kMaxEvents = 2000;
    const int kReadEvent = POLLIN;
    const int kWriteEvent = POLLOUT;

    class PollerBase : private util::NonCopyable 
    {
    public:
        PollerBase() : last_active_(-1) 
        {
            static std::atomic<int64_t> id(0);
            id_ = ++id;
        }
        virtual void AddChannel(Channel *ch) = 0;
        virtual void RemoveChannel(Channel *ch) = 0;
        virtual void UpdateChannel(Channel *ch) = 0;
        virtual void LoopOnce(int waitMs) = 0;
        virtual ~PollerBase(){};
    protected:
        int64_t id_;
        int last_active_;
    };

    
}