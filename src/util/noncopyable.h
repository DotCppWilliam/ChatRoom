#pragma once

namespace util
{
    class NonCopyable 
    {
    public:
        NonCopyable(const NonCopyable&) = delete;
        void operator=(NonCopyable&) = delete;
    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;
    };
}