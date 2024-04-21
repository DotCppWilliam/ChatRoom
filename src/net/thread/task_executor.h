#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <functional>
#include <type_traits>
#include <utility>

#include "../util/linked_list.h"
#include "../../util/noncopyable.h"

namespace net
{
    /**
     * @brief 线程负载计数器
     * 
     */
//////////////////////////////////////////////////////// CpuLoadCounter
    class ThreadLoadCounter
    {
    public:
        /**
         * @brief Construct a new Cpu Load Counter object
         * 
         * @param max_size 统计样本数量
         * @param max_usec 统计时间窗口,即最近max_usec的cpu负载率
         */
        ThreadLoadCounter(uint64_t max_size, uint64_t max_usec);
        ~ThreadLoadCounter() = default;

        void StartSleep();

        void SleepWakeup();

        int Load();
    private:
        struct TimeRecord
        {
            TimeRecord(uint64_t tm, bool sleep)
                :sleep_(sleep), time_(tm) {}

            bool sleep_;
            uint64_t time_;
        };
    private:
        bool        sleeping_;
        uint64_t    last_sleep_time_;
        uint64_t    last_wake_time_;
        uint64_t    max_size_;
        uint64_t    max_usec_;
        std::mutex  mutex_;
        util::LinkedList<TimeRecord> time_list_;
    };






//////////////////////////////////////////////////////// TaskCancelable
    class TaskCancelable : public util::NonCopyable
    {
    public:
        TaskCancelable() = default;
        virtual ~TaskCancelable() = default;
        virtual void Cancel() = 0;
    };

//////////////////////////////////////////////////////// TaskCancelableImpl
    template <class Result, class... Args>
    class TaskCancelableImpl;

    template <class Result, class... Args>
    class TaskCancelableImpl<Result(Args...)> : public TaskCancelable
    {
    public:
        using Ptr = std::shared_ptr<TaskCancelableImpl>;   
        using FuncType = std::function<Result(Args...)>;

        ~TaskCancelableImpl() = default;

        template <typename Func>
        TaskCancelableImpl(Func&& task)
        {
            strong_Task_ = std::make_shared<FuncType>(std::forward<Func>(task));
            weak_task_ = strong_Task_;
        }

        void Cancel() override 
        {
            strong_Task_ = nullptr;
        }

        operator bool() 
        {
            return strong_Task_ && *strong_Task_;
        }

        void operator=(std::nullptr_t)
        {
            strong_Task_ = nullptr;
        }

        Result operator()(Args... args) const 
        {
            auto strong_task = weak_task_.lock();
            if (strong_task && *strong_task)
                return (*strong_task)(std::forward<Args>(args)...);

            return DefaultValue<Result>();
        }

        template <typename T>
        static typename std::enable_if<std::is_void<T>::value, void>::type
            DefaultValue() {}
        
        template <typename T>
        static typename std::enable_if<std::is_pointer<T>::value, T>::type
            DefaultValue() { return nullptr; }
        
        template <typename T>
        static typename std::enable_if<std::is_integral<T>::value, T>::type
            DefaultValue() { return 0; }
    protected:
        std::weak_ptr<FuncType>     weak_task_;
        std::shared_ptr<FuncType>   strong_Task_;
    };







//////////////////////////////////////////////////////// TaskExecutorInterface
    using TaskIn = std::function<void()>;
    using Task = TaskCancelableImpl<void()>;

    class TaskExecutorInterface
    {
    public:
        TaskExecutorInterface() = default;
        virtual ~TaskExecutorInterface() = default;

        /**
         * @brief 异步执行任务
         * 
         * @param task 任务
         * @param may_sync 是否允许同步执行该任务
         * @return Task::Ptr 任务是否添加成功
         */
        virtual Task::Ptr Async(TaskIn task, bool may_sync = true) = 0;

        /**
         * @brief 最高优先级方式异步执行任务
         * 
         * @param task 任务
         * @param may_sync 是否允许同步执行任务
         * @return Task::Ptr 任务是否添加成功
         */
        virtual Task::Ptr AsyncFirst(TaskIn task, bool may_sync = true);

        /**
         * @brief 同步执行任务
         * 
         * @param task 
         */
        void Sync(const TaskIn& task);

        /**
         * @brief 最高优先级同步执行任务
         * 
         * @param task 
         */
        void SyncFirst(const TaskIn& task);
    };

//////////////////////////////////////////////////////// TaskExecutor
    class TaskExecutor : public ThreadLoadCounter, public TaskExecutorInterface
    {
    public:
        using Ptr = std::shared_ptr<TaskExecutor>;

        TaskExecutor(uint64_t max_size = 32, uint64_t max_usec = 2 * 1000 * 1000);
        ~TaskExecutor() = default;
    };










//////////////////////////////////////////////////////// TaskExecutorGetter
    class TaskExecutorGetter 
    {
    public:
        using Ptr = std::shared_ptr<TaskExecutorGetter>;

        virtual ~TaskExecutorGetter() = default;

        /**
         * @brief 获取任务执行器
         * 
         * @return TaskExecutor::Ptr 
         */
        virtual TaskExecutor::Ptr GetExecutor() = 0;

        /**
         * @brief 获取任务执行器个数
         * 
         * @return size_t 
         */
        virtual size_t GetExecutorSize() const = 0;
    };

//////////////////////////////////////////////////////// TaskExecutorGetterImpl
    class TaskExecutorGetterImpl : public TaskExecutorGetter
    {
    public:
        TaskExecutorGetterImpl() = default;
        ~TaskExecutorGetterImpl() = default;

        /**
         * @brief 根据线程负载情况,获取最空闲的任务执行器
         * 
         * @return TaskExecutor::Ptr 
         */
        TaskExecutor::Ptr GetExecutor() override;

        /**
         * @brief 获取所有线程的负载率
         * 
         * @return std::vector<int> 
         */
        std::vector<int> GetExecutorLoad();

        void Foreach(const std::function<void(const TaskExecutor::Ptr&)>& callback);

        /**
         * @brief 获取所有线程任务执行延时,单位毫秒
         *      通过此函数可以知道线程负载情况
         * 
         * @param callback 
         */
        void GetExecutorDelay(const std::function<void(const std::vector<int>&)>& callback);

        /**
         * @brief 获取线程执行器数量
         * 
         * @return size_t 
         */
        size_t GetExecutorSize() const override;
    protected:
        /**
         * @brief 
         * 
         * @param name 
         * @param size 
         * @param priority 优先级
         * @param register_thread  
         * @param enable_cpu_affinity 是否开启CPU亲和性
         * @return size_t 
         */
        size_t AddPoller(const std::string& name, size_t size, 
            int priority, 
            bool register_thread,
            bool enable_cpu_affinity = true);   
    protected:
        size_t                          thread_pos_;
        std::vector<TaskExecutor::Ptr>  threads_;
    };


}