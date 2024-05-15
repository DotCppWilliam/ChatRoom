#pragma once

#include <semaphore.h>
#include <cstddef>

namespace net 
{
    class Semaphore 
    {
    public:
        explicit Semaphore(size_t initial = 0)
        { sem_init(&sem_, 0, initial); }

        ~Semaphore()
        {  sem_destroy(&sem_); }

        /**
         * @brief 递增信号量
         * 
         * @param n 
         */
        void Post(size_t n = 1)
        {
            while (n--) 
                sem_post(&sem_);
        }
        
        /**
         * @brief 等待信号量
         * 
         */
        void Wait()
        { sem_wait(&sem_); }
    private:
        sem_t sem_;
    };
}