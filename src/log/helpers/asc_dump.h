#pragma once

#include <cstddef>
#include <ostream>
#include <sstream>

namespace helpers
{
    class AscDump
    {
    public:
        AscDump(const void* ptr, size_t size)
            : ptr_(static_cast<const char*>(ptr)),
            size_(size) {}

        friend std::ostream& operator<<(std::ostringstream& oss, const AscDump& dump);
    private:
        const char* ptr_;
        size_t size_;
    };

    inline AscDump AscDumpFunc(const void* ptr, size_t size) { return AscDump(ptr, size); }
    
    template <typename Container>
    inline AscDump AscDumpFunc(const Container& container) 
    { return AscDump(container.data(), container.size() * sizeof(*container.data())); }

    template <typename T, size_t N>
    inline AscDump AscDumpFunc(const T (&arr)[N]) { return AscDump(arr, N * sizeof(*arr)); }
}