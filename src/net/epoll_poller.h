#pragma once

#include "poller.h"
#include "channel.h"

#include <unordered_set>

namespace net 
{
    class EpollPoller : public PollerBase
    {
    public:
        EpollPoller();
        ~EpollPoller();

        void AddChannel(Channel *ch) override;
        void RemoveChannel(Channel *ch) override;
        void UpdateChannel(Channel *ch) override;
        void LoopOnce(int waitMs) override;
    private:
        int fd_;
        struct epoll_event active_events_[kMaxEvents];
        std::unordered_set<Channel*> live_channels_;
    };

    PollerBase *CreatePoller();
}