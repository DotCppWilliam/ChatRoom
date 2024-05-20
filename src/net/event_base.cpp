#include "event_base.h"
#include "concurrent_queue.h"
#include "epoll_poller.h"
#include "log.h"
#include "poller.h"
#include "threads.h"
#include "conn.h"
#include "concurrent_queue_impl.h"

#include <unordered_set>
#include <map>
#include <fcntl.h>

namespace net 
{
    class EventsImp 
    {
    public:
        EventsImp(EventBase *base, int task_capacity)
            : base_(base), poller_(CreatePoller()), exit_(false),
            next_timeout_(1 << 30),
            timer_seq_(0), idle_enabled(false) {}

        ~EventsImp()
        {
            delete poller_;
            ::close(wakeup_fds_[1]);
        }

        void Init();
        void CallIdles();
        IdleId RegisterIdle(int idle, const TcpConnPtr &conn, const TcpCallBack &cb);
        void UnregisterIdle(const IdleId &id);
        void UpdateIdle(const IdleId &id);
        void HandleTimeouts();
        void RefreshNearest(const TimerId *tid = NULL);
        void RepeatableTimeout(TimerRepeatable *tr);

        // eventbase functions
        EventBase &Exit() 
        {
            exit_ = true;
            Wakeup();
            return *base_;
        }
        bool Exited() { return exit_; }
        void SafeCall(Task &&task) 
        {
            tasks_.Enqueue(std::move(task));
            Wakeup();
        }
        void Loop();
        void LoopOnce(int waitMs) 
        {
            poller_->LoopOnce(std::min(waitMs, next_timeout_));
            HandleTimeouts();
        }
        void Wakeup() 
        {
            write(wakeup_fds_[1], "", 1);
        }

        bool Cancel(TimerId timerid);
        TimerId RunAt(int64_t milli, Task &&task, int64_t interval);

    private:
        EventBase *base_;
        PollerBase *poller_;
        std::atomic<bool> exit_;
        int wakeup_fds_[2];
        int next_timeout_;
        ConcurrentQueue<Task> tasks_;

        std::map<TimerId, TimerRepeatable> timer_reps_;
        std::map<TimerId, Task> timers_;
        std::atomic<int64_t> timer_seq_;
        // 记录每个idle时间（单位秒）下所有的连接。链表中的所有连接，最新的插入到链表末尾。连接若有活动，
        // 会把连接从链表中移到链表尾部，做法参考memcache
        std::unordered_map<int, std::list<IdleNode>> idle_conns_;
        std::unordered_set<TcpConnPtr> reconnect_conns_;
        bool idle_enabled;
    };


    void EventsImp::Init()
    {
        int ret = pipe(wakeup_fds_);
        ret = util::AddFdFlag(wakeup_fds_[0], FD_CLOEXEC);
        
        ret = util::AddFdFlag(wakeup_fds_[1], FD_CLOEXEC);
        LOG_FMT_VERBOSE_MSG("wakeup pipe created %d %d", wakeup_fds_[0], wakeup_fds_[1]);
        Channel *channel = new Channel(base_, wakeup_fds_[0], kReadEvent);
        channel->OnRead([=] {
            char buf[1024];
            int r = channel->Fd() >= 0 ? ::read(channel->Fd(), buf, sizeof buf) : 0;
            if (r > 0) 
            {
                Task task;
                while (tasks_.TryDequeue(task))
                    task();
            } 
            else if (r == 0) 
                delete channel;
            else 
                LOG_FMT_FATAL_MSG("wakeup channel read error %d %d %s", r, errno, strerror(errno));
        });
    }


    void EventsImp::CallIdles() 
    {
        int64_t now = util::TimeMilli() / 1000;
        for (auto &l : idle_conns_) 
        {
            int idle = l.first;
            auto& lst = l.second;
            while (lst.size()) 
            {
                IdleNode &node = lst.front();
                if (node.updated_ + idle > now) 
                    break;
                
                node.updated_ = now;
                lst.splice(lst.end(), lst, lst.begin());
                node.callback_(node.conn_);
            }
        }
    }


    IdleId EventsImp::RegisterIdle(int idle, const TcpConnPtr &con, const TcpCallBack &cb) 
    {
        if (!idle_enabled) 
        {
            base_->RunAfter(1000, [this] { CallIdles(); }, 1000);
            idle_enabled = true;
        }
        auto &lst = idle_conns_[idle];
        lst.push_back(IdleNode{con, util::TimeMilli() / 1000, std::move(cb)});
        LOG_VERBOSE_MSG("register idle");
        return IdleId(new IdleIdImp(&lst, --lst.end()));
    }


    void EventsImp::UnregisterIdle(const IdleId &id) 
    {
        LOG_VERBOSE_MSG("unregister idle");
        id->list_->erase(id->iter_);
    }


    void EventsImp::UpdateIdle(const IdleId &id) 
    {
        LOG_VERBOSE_MSG("update idle");
        id->iter_->updated_ = util::TimeMilli() / 1000;
        id->list_->splice(id->list_->end(), *id->list_, id->iter_);
    }

    void EventsImp::HandleTimeouts() 
    {
        int64_t now = util::TimeMilli();
        TimerId tid{now, 1L << 62};
        while (timers_.size() && timers_.begin()->first < tid) 
        {
            Task task = std::move(timers_.begin()->second);
            timers_.erase(timers_.begin());
            task();
        }
        RefreshNearest();
    }

    void EventsImp::RefreshNearest(const TimerId *tid) 
    {
        if (timers_.empty())
            next_timeout_ = 1 << 30;
        else 
        {
            const TimerId &t = timers_.begin()->first;
            next_timeout_ = t.first - util::TimeMilli();
            next_timeout_ = next_timeout_ < 0 ? 0 : next_timeout_;
        }
    }


    void EventsImp::RepeatableTimeout(TimerRepeatable *tr) 
    {
        tr->at += tr->interval;
        tr->timerid = {tr->at, ++timer_seq_};
        timers_[tr->timerid] = [this, tr] { RepeatableTimeout(tr); };
        RefreshNearest(&tr->timerid);
        tr->cb();
    }


    void EventsImp::Loop() 
    {
        while (!exit_)
            LoopOnce(10000);
        timer_reps_.clear();
        timers_.clear();
        idle_conns_.clear();

        //重连的连接无法通过channel清理，因此单独清理
        for (auto recon : reconnect_conns_) 
        {  
            recon->Cleanup(recon);
        }
        LoopOnce(0);
    }


    bool EventsImp::Cancel(TimerId timerid) 
    {
        if (timerid.first < 0) 
        {
            auto it = timer_reps_.find(timerid);
            auto ptimer = timers_.find(it->second.timerid);
            if (ptimer != timers_.end()) 
                timers_.erase(ptimer);
            
            timer_reps_.erase(it);
            return true;
        } 
        else 
        {
            auto p = timers_.find(timerid);
            if (p != timers_.end()) 
            {
                timers_.erase(p);
                return true;
            }
            return false;
        }
    }


    TimerId EventsImp::RunAt(int64_t milli, Task &&task, int64_t interval) 
    {
        if (exit_) 
            return TimerId();
        
        if (interval) 
        {
            TimerId tid{-milli, ++timer_seq_};
            TimerRepeatable &rtr = timer_reps_[tid];
            rtr = {milli, interval, {milli, ++timer_seq_}, std::move(task)};
            TimerRepeatable *tr = &rtr;
            timers_[tr->timerid] = [this, tr] { RepeatableTimeout(tr); };
            RefreshNearest(&tr->timerid);
            return tid;
        } 
        else 
        {
            TimerId tid{milli, ++timer_seq_};
            timers_.insert({tid, std::move(task)});
            RefreshNearest(&tid);
            return tid;
        }
    }




}