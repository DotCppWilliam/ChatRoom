#pragma once

#include "util.h"
#include "noncopyable.h"

#include <cstdint>
#include <memory>
#include <atomic>
#include <functional>
#include <utility>


struct TcpConn;
struct TcpServer;
struct Slice;



namespace net 
{
    using Task = std::function<void()>;
    struct IdleIdImp;

    using TimerId = std::pair<int64_t, int64_t>;
    using IdleId = std::unique_ptr<IdleIdImp>;
    
    struct EventBase;
    struct EventBases : private util::NonCopyable
    {
        virtual EventBase* AllocBase() = 0;
    };

    struct EventsImp;
    struct EventBase : public EventBases
    {
        // taskCapacity指定任务队列的大小，0无限制
        EventBase(int taskCapacity = 0);
        ~EventBase();
        //处理已到期的事件,waitMs表示若无当前需要处理的任务，需要等待的时间
        void LoopOnce(int waitMs);
        //进入事件处理循环
        void Loop();
        //取消定时任务，若timer已经过期，则忽略
        bool Cancel(TimerId timerid);
        //添加定时任务，interval=0表示一次性任务，否则为重复任务，时间为毫秒
        TimerId RunAt(int64_t milli, const Task &task, int64_t interval = 0) { return RunAt(milli, Task(task), interval); }
        TimerId RunAt(int64_t milli, Task &&task, int64_t interval = 0);
        TimerId RunAfter(int64_t milli, const Task &task, int64_t interval = 0) { return RunAt(util::TimeMilli() + milli, Task(task), interval); }
        TimerId RunAfter(int64_t milli, Task &&task, int64_t interval = 0) { return RunAt(util::TimeMilli() + milli, std::move(task), interval); }

        //下列函数为线程安全的

        //退出事件循环
        EventBase &Exit();
        //是否已退出
        bool Exited();
        //唤醒事件处理
        void Wakeup();
        //添加任务
        void SafeCall(Task &&task);
        void SafeCall(const Task &task) { SafeCall(Task(task)); }
        //分配一个事件派发器
        virtual EventBase *AllocBase() { return this; }

    public:
        std::unique_ptr<EventsImp> imp_;
    };


    //多线程的事件派发器
    struct MultiBase : public EventBases 
    {
        MultiBase(int sz) : id_(0), bases_(sz) {}
        virtual EventBase *AllocBase() 
        {
            int c = id_++;
            return &bases_[c % bases_.size()];
        }
        void Loop();
        MultiBase &Exit() 
        {
            for (auto &b : bases_) 
            {
                b.Exit();
            }
            return *this;
        }

    private:
        std::atomic<int> id_;
        std::vector<EventBase> bases_;
    };








    struct TimerRepeatable 
    {
        int64_t at;  // current timer timeout timestamp
        int64_t interval;
        TimerId timerid;
        Task cb;
    };

    
    

    struct AutoContext : util::NonCopyable 
    {
        AutoContext() : ctx_(0) {}
        template <class T>
        T &Context() 
        {
            if (ctx_ == NULL) 
            {
                ctx_ = new T();
                ctx_del_ = [this] { delete (T *) ctx_; };
            }
            return *(T *) ctx_;
        }
        ~AutoContext() 
        {
            if (ctx_)
                ctx_del_();
        }

        void *ctx_;
        Task ctx_del_;
    };

}