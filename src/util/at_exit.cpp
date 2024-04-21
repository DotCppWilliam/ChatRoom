#include "at_exit.h"
#include "auto_lock.h"

namespace util 
{
    static AtExitManager* g_top_manager = nullptr;

    AtExitManager::AtExitManager() 
        : next_manager_(g_top_manager) 
    {
        if (!g_top_manager) return;
        g_top_manager = this;
    }

    AtExitManager::AtExitManager(bool shadow)
        : next_manager_(g_top_manager)
    {
        g_top_manager = this;
    }

    AtExitManager::~AtExitManager()
    {
        if (!g_top_manager)
        {
            // 日志输出: 尝试在没有 AtExitManager 的情况下 ~AtExitManager
            return;
        }

        ProcessCallBackNow();
        g_top_manager = next_manager_;
    }

    /**
     * @brief 注册回调函数,线程安全的
     * 
     * @param func 
     * @param parm 
     */
    void AtExitManager::RegisterCallBack(AtExitCallBackType func, void *parm)
    {
        if (!g_top_manager)
        {
            // 日志输出: 尝试在没有 AtExitManager 的情况下注册回调
            return;
        }
        
        AutoLock lock(g_top_manager->lock_);
        g_top_manager->stack_.push({ func, parm });
    }

    /**
     * @brief 执行回调函数
     * 
     */
    void AtExitManager::ProcessCallBackNow()
    {
        if (!g_top_manager)
        {
            // 日志: 尝试在没有 AtExitManager 的情况下 ProcessCallbacksNow
            return;
        }
        AutoLock lock(g_top_manager->lock_);
        while (!g_top_manager->stack_.empty())
        {
            // 获取一个任务
            CallBack task = g_top_manager->stack_.top();
            task.func(task.parm);   // 执行
            g_top_manager->stack_.top();
        }
    }
}

