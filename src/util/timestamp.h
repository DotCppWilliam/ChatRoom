#pragma once

#include <ctime>
#include <sys/time.h>

namespace util 
{
    struct Time 
    {
        time_t time;
        unsigned short millisecond;
    };  
    inline void LocalTime_s(struct tm* t, const time_t* time)
    {
        ::localtime_r(time, t);
    }   
    inline void GmTime_s(struct tm* t, const time_t* time)
    {
        ::gmtime_r(time, t);
    }   
    inline void Ftime(Time* t)
    {
        timeval tv;
        ::gettimeofday(&tv, nullptr);   
        t->time = tv.tv_sec;
        t->millisecond = static_cast<unsigned short>(tv.tv_usec / 1000);
    }
}