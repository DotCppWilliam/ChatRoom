#pragma once

#include "log.h"
#include "record.h"
#include "severity.h"

#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>


namespace net 
{
    class ThreadGroup
    {
        using UnOrderedMap = std::unordered_map<std::thread::id, std::shared_ptr<std::thread>>;
    public:
        ThreadGroup(const ThreadGroup&) = delete;
        ThreadGroup& operator=(const ThreadGroup&) = delete;

        ThreadGroup() {}
        ~ThreadGroup() { threads_.clear(); }

        bool IsThisThreadIn()
        {
            auto thread_id = std::this_thread::get_id();
            if (thread_id == thread_id_)
                return true; 

            return threads_.find(thread_id) != threads_.end();
        }

        bool IsThisThreadIn(std::thread* thread_ptr)
        {
            if (thread_ptr) return false;

            auto it = threads_.find(thread_ptr->get_id());
            return it != threads_.end();
        }

        template <typename Func>
        std::thread* CreateThread(Func&& thread_func)
        {
            auto thread_new = std::make_shared<std::thread>(std::forward<Func>(thread_func));
            thread_id_ = thread_new->get_id();
            threads_[thread_id_] = thread_new;
            return thread_new.get();
        }

        void RemoveThread(std::thread* thread_ptr)
        {
            auto it = threads_.find(thread_ptr->get_id());
            if (it != threads_.end())
                threads_.erase(it);
        }

        void JoinAll()
        {
            if (IsThisThreadIn())
            {
                LOG_FATAL_MSG("尝试将自身加入到 thread_group 中");
                throw std::runtime_error("尝试将自身加入到 thread_group 中");
            }
            for (auto& item : threads_)
            {
                if (item.second->joinable())
                    item.second->join();
            }
        }

        size_t size() const 
        {
            return threads_.size();
        }
    private:
        std::thread::id     thread_id_;
        UnOrderedMap        threads_;
    };
}