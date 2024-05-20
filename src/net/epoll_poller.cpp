#include "epoll_poller.h"
#include "log.h"

#include <sys/epoll.h>
#include <cstring>

namespace net 
{
    EpollPoller::EpollPoller()
    {
        fd_ = epoll_create1(EPOLL_CLOEXEC);
        LOG_FMT_VERBOSE_MSG("epoll %d created\n", fd_);
    }

    EpollPoller::~EpollPoller()
    {
        LOG_FMT_VERBOSE_MSG("Destroying epoll %d\n", fd_);
        while (live_channels_.size())
            (*live_channels_.begin())->Close();
        LOG_FMT_VERBOSE_MSG("destroyed epoll %d\n", fd_);
    }

    void EpollPoller::AddChannel(Channel *ch) 
    {
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = ch->Events();
        ev.data.ptr = ch;
        LOG_FMT_VERBOSE_MSG("adding channel %lld Fd %d events %d epoll %d", (long long) ch->Id(), ch->Fd(), 
            ev.events, fd_);
        int r = epoll_ctl(fd_, EPOLL_CTL_ADD, ch->Fd(), &ev);
        live_channels_.insert(ch);
    }

    void EpollPoller::UpdateChannel(Channel *ch) 
    {
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = ch->Events();
        ev.data.ptr = ch;
        LOG_FMT_VERBOSE_MSG("modifying channel %lld Fd %d events read %d write %d epoll %d", (long long) ch->Id(), 
            ch->Fd(), ev.events & POLLIN, ev.events & POLLOUT, fd_);
        epoll_ctl(fd_, EPOLL_CTL_MOD, ch->Fd(), &ev);
    }

    void EpollPoller::RemoveChannel(Channel *ch) 
    {
        LOG_FMT_VERBOSE_MSG("deleting channel %lld Fd %d epoll %d", (long long) ch->Id(), ch->Fd(), fd_);
        live_channels_.erase(ch);
        for (int i = last_active_; i >= 0; i--) 
        {
            if (ch == active_events_[i].data.ptr) 
            {
                active_events_[i].data.ptr = NULL;
                break;
            }
        }
    }



    void EpollPoller::LoopOnce(int wait_ms) 
    {
        int64_t ticks = util::TimeMilli();
        last_active_ = epoll_wait(fd_, active_events_, kMaxEvents, wait_ms);
        int64_t used = util::TimeMilli() - ticks;
        LOG_FMT_VERBOSE_MSG("epoll wait %d return %d errno %d used %lld millsecond", wait_ms, 
            last_active_, errno, (long long) used);

        while (--last_active_ >= 0) 
        {
            int i = last_active_;
            Channel *ch = (Channel *) active_events_[i].data.ptr;
            int events = active_events_[i].events;
            if (ch) 
            {
                if (events & (kReadEvent | POLLERR)) 
                {
                    LOG_FMT_VERBOSE_MSG("channel %lld Fd %d handle read", (long long) ch->Id(), ch->Fd());
                    ch->HandleRead();
                } 
                else if (events & kWriteEvent) 
                {
                    LOG_FMT_VERBOSE_MSG("channel %lld Fd %d handle write", (long long) ch->Id(), ch->Fd());
                    ch->HandleWrite();
                } 
                else 
                {
                    LOG_FATAL_MSG("unexpected poller events");
                }
            }
        }
    }




    PollerBase *CreatePoller()
    {
         return new EpollPoller;
    }
}