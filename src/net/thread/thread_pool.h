#pragma once

#include "log.h"
#include "task_executor.h"
#include "thread_group.h"
#include "task_queue.h"
#include "../util/util.h"


#include <bits/types/struct_sched_param.h>
#include <exception>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <thread>

namespace net 
{
    class ThreadPool : public TaskExecutor
    {
    public:
        enum Priority 
        {
            PRIORITY_LOWEST = 0,
            PRIORITY_LOW,
            PRIORITY_NORMAL,
            PRIORITY_HIGH,
            PRIORITY_HIGHEST
        };

        ThreadPool(int num = 1, Priority priority = PRIORITY_HIGHEST,
            bool auto_run = true, bool set_affinity = true, 
            const std::string& pool_name = "thread pool")
            
        {
            thread_num_ = num;
            on_setup_ = 
                [pool_name, priority, set_affinity](int index)
                {
                    std::string name = pool_name + ' ' + std::to_string(index);
                    SetPriority(priority);
                    util::SetThreadName(name.data());

                    if (set_affinity)
                        util::SetThreadAffinity(index % std::thread::hardware_concurrency());
                };
                if (auto_run) Start();
        }

        ~ThreadPool() 
        {
            Shutdown();
            Wait();
        }

        Task::Ptr Async(TaskIn task, bool may_sync = true) override
        {
            if (may_sync && thread_group_.IsThisThreadIn())
            {
                task();
                return nullptr;
            }
            auto ret = std::make_shared<Task>(std::move(task));
            task_queue_.PushTask(ret);
            return ret;
        }

        Task::Ptr AsyncFirst(TaskIn task, bool may_sync = true) override
        {
            if (may_sync && thread_group_.IsThisThreadIn())
            {
                task();
                return nullptr;
            }

            auto ret = std::make_shared<Task>(std::move(task));
            task_queue_.PushTaskFirst(ret);
            return ret;
        }

        size_t size() { return task_queue_.size(); }

        static bool SetPriority(Priority priority = PRIORITY_NORMAL, 
            std::thread::native_handle_type thread_id = 0)
        {
            static int min = sched_get_priority_min(SCHED_FIFO);
            if (min == -1) return false;

            static int max = sched_get_priority_max(SCHED_FIFO);
            if (max == -1) return false;

            static int priorities[] = {
                min, min + (max - min) / 4, 
                min + (max - min) / 2,
                min + (max - min) * 3 / 4, max};
            if (thread_id == 0)
                thread_id = pthread_self();
                
            struct sched_param params;
            params.sched_priority = priorities[priority];
            return pthread_setschedparam(thread_id, SCHED_FIFO, &params) == 0;
        }

        void Start()
        {
            if (thread_num_ <= 0) return;

            size_t total = thread_num_ - thread_group_.size();
            for (size_t i = 0; i < total; i++)
                thread_group_.CreateThread([this, i]() { return Run(i); });
        }
    private:
        void Run(size_t index)
        {
            on_setup_(index);

            Task::Ptr task;
            while (true)
            {
                StartSleep();
                if (!task_queue_.GetTask(task))
                    break;  // 空任务
                SleepWakeup();
                try {
                    (*task)();
                    task = nullptr;
                } 
                catch (std::exception& e) 
                {
                    LOG_FMT_FATAL_MSG("ThreadPool类捕获一个异常: %s\n", e.what());
                }
            }
        }

        void Wait()
        { thread_group_.JoinAll(); }

        void Shutdown()
        { task_queue_.PushExit(thread_num_); }
    private:
        size_t                      thread_num_;
        ThreadGroup                 thread_group_;
        TaskQueue<Task::Ptr>        task_queue_;
        std::function<void(int)>    on_setup_;
    };
}