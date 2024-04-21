#pragma once

#include <cstddef>
#include <sstream>

namespace helpers
{
    class HexDump
    {
        friend std::ostringstream& operator<<(std::ostringstream& oss, const HexDump&);
    public:
        HexDump(const void* ptr, size_t size)
            : ptr_(static_cast<const unsigned char*>(ptr)),
            size_(size),
            group_(8),
            digit_separator_(" "),
            group_separator_("  ") {}

        HexDump& Group(size_t group)
        {
            group_ = group;
            return *this;
        }

        HexDump& Separator(const char* digit_separator)
        {
            digit_separator_ = digit_separator;
            return *this;
        }

        HexDump& Separator(const char* digit_separator, const char* group_separator)
        {
            digit_separator_ = digit_separator;
            group_separator_ = group_separator;
            return *this;
        }
        
    private:
        const unsigned char* ptr_;
        size_t size_;
        size_t group_;
        const char* digit_separator_;
        const char* group_separator_;
    };

    inline HexDump HexDumpFunc(const void* ptr, size_t size) { return HexDump(ptr, size); }

    template <typename Container>
    inline HexDump HexDumpFunc(const Container& container) 
    { return HexDump(container.data(), container.size() * sizeof(*container.data())); }

    template <typename T, size_t N>
    inline HexDump HexDumpFunc(const T (&arr)[N]) { return HexDump(arr, N * sizeof(*arr)); }
}