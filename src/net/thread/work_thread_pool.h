#pragma once

#include "task_executor.h"
#include "../poller/event_poller.h"

#include <memory>
namespace net 
{
    class WorkThreadPool : public std::enable_shared_from_this<WorkThreadPool>, 
                            public TaskExecutorGetterImpl
    {
    public:
        using Ptr = std::shared_ptr<WorkThreadPool>;

        ~WorkThreadPool() override = default;

        /**
         * @brief 获取单例
         * 
         * @return WorkThreadPool& 
         */
        static WorkThreadPool& GetInstance();

        /**
         * @brief 设置EventPoller的个数,在WorkThreadPool单例创建前有效
         *      在不调用此方法的情况下,默认创建thread::hardware_concurrency()个EventPoller实例
         * 
         * @param size EventPoller个数,如果为0则为 thread::hardware_concurrency()
         */
        static void SetPoolSize(size_t size = 0);
        
        /**
         * @brief 内部创建线程是否设置CPU亲和性,默认开启
         * 
         * @param enable 
         */
        static void enableCpuAffinity(bool enable);

        /**
         * @brief 获取第一个Poller实例
         * 
         * @return EventPoller::Ptr 
         */
        EventPoller::Ptr GetFirstPoller();

        /**
         * @brief 根据负载情况获取轻负载实例,
         *          如果优先返回当前线程,那么会返回当前线程
         *          返回当前线程目的就是为了提高线程安全性
         * @return EventPoller::Ptr 
         */
        EventPoller::Ptr GetPoller();
    protected:
        WorkThreadPool();
    };
}