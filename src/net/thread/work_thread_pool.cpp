#include "work_thread_pool.h"
#include "event_poller.h"
#include "thread_pool.h"
#include <memory>

namespace net 
{
    static size_t kPoolSize = 0;
    static bool kEnableCpuAffinity = true;

    EventPoller::Ptr WorkThreadPool::GetFirstPoller()
    {
        return std::static_pointer_cast<EventPoller>(threads_.front());
    }

    EventPoller::Ptr WorkThreadPool::GetPoller()
    {
        return std::static_pointer_cast<EventPoller>(GetExecutor());
    }

    WorkThreadPool::WorkThreadPool()
    {
        AddPoller("work poller", kPoolSize, 
            ThreadPool::PRIORITY_LOWEST, 
            false, kEnableCpuAffinity);
    }

    void WorkThreadPool::SetPoolSize(size_t size)
    {
        kPoolSize = size;
    }

    void WorkThreadPool::enableCpuAffinity(bool enable)
    {
        kEnableCpuAffinity = enable;
    }
}