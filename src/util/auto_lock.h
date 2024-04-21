#pragma once

#include <pthread.h>

namespace util 
{
    class Mutex 
    {
    public:
        Mutex()
        {
            pthread_mutex_init(&mutex_, nullptr);
        }

        ~Mutex()
        {
            pthread_mutex_destroy(&mutex_);
        }

        void Lock()
        {
            pthread_mutex_lock(&mutex_);
        }

        void Unlock()
        {
            pthread_mutex_unlock(&mutex_);
        }

        bool TryLock()
        {
            return pthread_mutex_trylock(&mutex_);
        }
    private:
        pthread_mutex_t mutex_; 
    };


    class Lock : public Mutex 
    {
    public:
        Lock() {}
        ~Lock() {}

        void Acquire() { Lock(); }  // 获得锁
        void Release() { Unlock(); }// 释放锁
        bool Try() { return TryLock(); }
        void AssertAcquired() const {}
    };

    class AutoLock 
    {
    public:
        struct AlreadyAcquired {};

        explicit AutoLock(Lock& lock) 
            : lock_(lock) 
        {
            lock_.Acquire();    // 持有锁
        }

        AutoLock(Lock& lock, const AlreadyAcquired&)
            : lock_(lock)
        {
            lock_.AssertAcquired(); // 已经上锁
        }

        ~AutoLock()
        {
            lock_.AssertAcquired();
            lock_.Release();
        }
    private:
        Lock& lock_;
    };


    class AutoUnLock
    {
    public:
        explicit AutoUnLock(Lock& lock)
            : lock_(lock) 
        {
            lock_.AssertAcquired();
            lock_.Release();
        }
        ~AutoUnLock()
        {
            lock_.Acquire();
        }
    private:
        Lock& lock_;
    };
}


