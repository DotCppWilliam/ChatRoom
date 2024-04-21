#include "hex_dump.h"
#include <iomanip>

namespace helpers
{
    std::ostringstream& operator<<(std::ostringstream& oss, const HexDump& hex_dump)
    {
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < hex_dump.size_; )
        {
            oss << std::setw(2) << static_cast<int>(hex_dump.ptr_[i]);

            if (++i < hex_dump.size_)
            {
                if (hex_dump.group_ > 0 && i % hex_dump.group_ == 0)
                    oss << hex_dump.group_separator_;
                else
                    oss << hex_dump.digit_separator_;
            }
        }
        return oss;
    }
}