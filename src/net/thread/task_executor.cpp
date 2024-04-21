#include "task_executor.h"
#include "once_token.h"
#include "time_ticker.h"
#include "util.h"
#include "custom_semaphore.h"
#include "event_poller.h"
#include "thread_pool.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <thread>

namespace net 
{

//////////////////////////////////////////// ThreadLoadCounter实现
    ThreadLoadCounter::ThreadLoadCounter(uint64_t max_size, uint64_t max_usec)
        : max_size_(max_size), max_usec_(max_usec), sleeping_(true)
    {
        last_sleep_time_ = last_wake_time_ = util::GetCurrMicrosecond();
        max_size_ = max_size;
        max_usec_ = max_usec;
    }

    void ThreadLoadCounter::StartSleep()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sleeping_ = true;
        auto current_time = util::GetCurrMicrosecond();
        auto run_time = current_time - last_wake_time_;
        last_sleep_time_ = current_time;
        time_list_.emplace_back(run_time, false);
        if (time_list_.size() > max_size_)
            time_list_.pop_front();
    }

    void ThreadLoadCounter::SleepWakeup()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sleeping_ = false;
        auto current_time = util::GetCurrMicrosecond();
        auto sleep_time = current_time - last_sleep_time_;
        last_wake_time_ = current_time;

        time_list_.emplace_back(sleep_time, true);
        if (time_list_.size() > max_size_)
            time_list_.pop_front();
    }

    int ThreadLoadCounter::Load()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t total_sleep_time = 0;
        uint64_t total_run_time = 0;
        time_list_.Foreach([&](const TimeRecord& record)
        {
            if (record.sleep_)
                total_sleep_time = record.time_;
            else
                total_run_time = record.time_;
        });

        if (sleeping_)
            total_sleep_time += (util::GetCurrMicrosecond() - last_sleep_time_);
        else
            total_run_time += (util::GetCurrMicrosecond() - last_wake_time_);

        uint64_t total_time = total_run_time + total_sleep_time;
        while ((time_list_.size() != 0) && 
            (total_time > max_usec_ || time_list_.size() > max_size_))
        {
            TimeRecord& record = time_list_.front();
            if (record.sleep_)
                total_sleep_time -= record.time_;
            else
                total_run_time -= record.time_;

            total_time -= record.time_;
            time_list_.pop_front();
        }

        if (total_time == 0)
            return 0;

        return (int)(total_run_time * 100 / total_time);
    }



//////////////////////////////////////////// TaskExecutorInterface实现
    Task::Ptr TaskExecutorInterface::AsyncFirst(TaskIn task, bool may_sync)
    {
        return Async(std::move(task), may_sync);
    }


    void TaskExecutorInterface::Sync(const TaskIn& task)
    {
        Semaphore sem;
        auto ret = Async([&](){
            uitl::OnceToken token(nullptr, [&](){
                sem.Post();
            });
            task();
        });
        if (ret && *ret)
            sem.Wait();
    }


    void TaskExecutorInterface::SyncFirst(const TaskIn& task)
    {
        Semaphore sem;
        auto ret = Async([&](){
            uitl::OnceToken token(nullptr, [&](){
                sem.Post();
            });
            task();
        });

        if (ret && *ret)
            sem.Wait();
    }


//////////////////////////////////////////// TaskExecutor实现
    TaskExecutor::TaskExecutor(uint64_t max_size, uint64_t max_usec)
        : ThreadLoadCounter(max_size, max_usec) {}

    TaskExecutor::Ptr TaskExecutorGetterImpl::GetExecutor()
    {
        auto thread_pos = thread_pos_;
        if (thread_pos >= threads_.size())
            thread_pos = 0;

        TaskExecutor::Ptr executor_min_load = threads_[thread_pos];
        auto min_load = executor_min_load->Load();

        for (size_t i = 0; i < threads_.size(); i++)
        {
            ++thread_pos;
            if (thread_pos >= threads_.size())
                thread_pos = 0;

            auto th = threads_[thread_pos];
            auto load = th->Load();

            if (load < min_load)
            {
                min_load = load;
                executor_min_load = th;
            }   

            if (min_load == 0)
                break;
        }
        thread_pos_ = thread_pos;
        return executor_min_load;
    }

    std::vector<int> TaskExecutorGetterImpl::GetExecutorLoad()
    {
        std::vector<int> vec(threads_.size());
        int i = 0;
        for (auto& executor : threads_)
            vec[i++] = executor->Load();
        return vec;
    }

    void TaskExecutorGetterImpl::GetExecutorDelay(
        const std::function<void(const std::vector<int>&)>& callback)
    {
        std::shared_ptr<std::vector<int>> delay_vec = 
            std::make_shared<std::vector<int>>(threads_.size());

        std::shared_ptr<void> finished(nullptr, [callback, delay_vec](void*){
            callback((*delay_vec));
        });

        int index = 0;
        for (auto& th : threads_)
        {
            std::shared_ptr<util::Ticker> delay_ticker = std::make_shared<util::Ticker>();
            th->Async([finished, delay_vec, index, delay_ticker](){
                (*delay_vec)[index] = (int)delay_ticker->ElapsedTime();
            }, false);
            ++index;
        }
    }

    void TaskExecutorGetterImpl::Foreach(const std::function<void(const TaskExecutor::Ptr&)>& callback)
    {
        for (auto& th : threads_)
            callback(th);
    }

    size_t TaskExecutorGetterImpl::GetExecutorSize() const 
    {
        return threads_.size();
    }

    size_t TaskExecutorGetterImpl::AddPoller(const std::string& name, size_t size, 
            int priority, 
            bool register_thread,
            bool enable_cpu_affinity)
    {
        auto cpus = std::thread::hardware_concurrency();
        size = size > 0 ? size : cpus;
        
        for (size_t i = 0; i < size; i++)
        {
            auto full_name = name + " " + std::to_string(i);
            auto cpu_index = i % cpus;
            EventPoller::Ptr poller(new EventPoller(full_name));
            poller->RunInLoop(false, register_thread);
            poller->Async([cpu_index, full_name, priority, enable_cpu_affinity](){
                // 设置线程优先级
                ThreadPool::SetPriority((ThreadPool::Priority) priority);

                // 设置线程名
                util::SetThreadName(full_name.c_str());

                // 设置CPU亲和性
                if (enable_cpu_affinity)
                    util::SetThreadAffinity(cpu_index);
            });
            threads_.emplace_back(std::move(poller));
        }
        return size;
    }
}