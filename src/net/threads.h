#pragma once

#include "noncopyable.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <limits>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

namespace net 
{

    template <typename T>
    struct SafeQueue : private std::mutex, private util::NonCopyable 
    {
        static const int wait_infinite = std::numeric_limits<int>::max();
        // 0 不限制队列中的任务数
        SafeQueue(size_t capacity = 0) : capacity_(capacity), exit_(false) {}
        //队列满则返回false
        bool Push(T &&v);
        //超时则返回T()
        T PopWait(int waitMs = wait_infinite);
        //超时返回false
        bool PopWait(T *v, int waitMs = wait_infinite);

        size_t Size();
        void Exit();
        bool Exited() { return exit_; }

    private:
        std::list<T> items_;
        std::condition_variable ready_;
        size_t capacity_;
        std::atomic<bool> exit_;
        void wait_ready(std::unique_lock<std::mutex> &lk, int waitMs);
    };

    typedef std::function<void()> Task;
    extern template class SafeQueue<Task>;

    // struct ThreadPool : private util::NonCopyable  
    // {
    //     //创建线程池
    //     ThreadPool(int threads, int taskCapacity = 0, bool start = true);
    //     ~ThreadPool();
    //     void Start();
    //     ThreadPool &Exit() 
    //     {
    //         tasks_.Exit();
    //         return *this;
    //     }
    //     void Join();

    //     //队列满返回false
    //     bool AddTask(Task &&task);
    //     bool AddTask(Task &task) { return AddTask(Task(task)); }
    //     size_t TaskSize() { return tasks_.Size(); }

    // private:
    //     SafeQueue<Task> tasks_;
    //     std::vector<std::thread> threads_;
    // };
}