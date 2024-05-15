#pragma once


#include <cstddef>

#include "concurrent_queue_impl.h"

namespace net 
{
    template <typename T>
    class TaskQueue
    {
    public:
        template <typename Task>
        void PushTask(Task&& task_func)
        {
            queue_.Enqueue(task_func);
        }

        template <typename Task>
        void PushTaskFirst(Task&& task_func);

        void PushExit(size_t n);

        bool GetTask(T& task);

        size_t size() const;
    private:
        ConcurrentQueue<T> queue_;
    };
}