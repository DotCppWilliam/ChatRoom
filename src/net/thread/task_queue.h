#pragma once


/*
    想实现成一个高效的无锁队列
*/

#include <cstddef>
namespace net 
{
    template <typename T>
    class TaskQueue
    {
    public:
        template <typename Task>
        void PushTask(Task&& task_func);

        template <typename Task>
        void PushTaskFirst(Task&& task_func);

        void PushExit(size_t n);

        bool GetTask(T& task);

        size_t size() const;
    private:

    };
}