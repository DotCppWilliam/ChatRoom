#pragma once

#include "task_executor.h"

#include <cstdint>
#include <functional>
#include <memory>


namespace net
{
    class EventPoller : public TaskExecutor,
                        public std::enable_shared_from_this<EventPoller> /* 用于获取自己的shared_ptr */
    {
        friend class TaskExecutorGetterImpl;
    public:
        using Ptr = std::shared_ptr<EventPoller>;
        using PollEventCallBack = std::function<void(int event)>;
        using PollCompleteCallBack = std::function<void(bool success)>;
        using DelayTask = TaskCancelableImpl<uint64_t(void)>;
    public:
        Task::Ptr Async(TaskIn task, bool may_sync = true) override;

        Task::Ptr AsyncFirst(TaskIn task, bool may_sync = true) override;
    private:
        EventPoller(std::string name);
        void RunInLoop(bool blocked, bool ref_self);
    };

}