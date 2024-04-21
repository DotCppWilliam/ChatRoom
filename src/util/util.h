#pragma once

#include <cstdint>

namespace util
{
    void SetThreadName(const char* name);


    bool SetThreadAffinity(int index);

    uint64_t GetCurrMicrosecond(bool system_time = false);

}
