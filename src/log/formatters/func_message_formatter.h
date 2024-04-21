#pragma once

#include "record.h"

#include <sstream>
#include <string>

namespace logging 
{

    class FuncMessageFormatter
    {
    public:
        static std::string Header()
        {
            return std::string();
        }

        static std::string Format(const Record& record)
        {
            std::ostringstream oss;
            oss << record.get_func() << "@" << record.get_line() << ": " 
                << record.get_message() << "\n";
            return oss.str();
        }
    };
}