#include "asc_dump.h"

namespace helpers
{
    std::ostream& operator<<(std::ostringstream& oss, const AscDump& dump)
    {
        for (size_t i = 0; i < dump.size_; ++i)
            oss << (std::isprint(dump.ptr_[i] ? dump.ptr_[i] : '.'));
        return oss;
    }
}
