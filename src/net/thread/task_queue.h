#pragma once


#include "custom_semaphore.h"
#include <cstddef>
#include <linked_list.h>
#include <mutex>


namespace net 
{
    template <typename T>
    class TaskQueue
    {
    public:
        /**
         * @brief 压入任务至队尾
         * 
         * @param task_func 
         */
        template <typename Task>
        void PushTask(Task&& task_func)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.emplace_back(std::forward<Task>(task_func));
            }
            sem_.Post();
        }


        /**
         * @brief 将任务压入至队头
         * 
         * @param task_func 
         */
        template <typename Task>
        void PushTaskFirst(Task&& task_func)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.emplace_front(std::forward<Task>(task_func));
            }
            sem_.Post();
        }


        /**
         * @brief 清空任务队列
         * 
         * @param n 
         */
        void PushExit(size_t n)
        {
            sem_.Post(n);
        }


        /**
         * @brief 从任务队列中获取一个任务,由执行线程执行
         * 
         * @param task 
         * @return true 
         * @return false 
         */
        bool GetTask(T& task)
        {
            sem_.Wait();
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty())
                return false;

            task = std::move(queue_.front());
            queue_.pop_front();
            return true;
        }

        size_t size() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }
    private:
        util::LinkedList<T> queue_;
        mutable std::mutex mutex_;
        Semaphore sem_;
    };
}