#pragma once

#include "record.h"

#include <string>


namespace logging 
{

    class MessageOnlyFormatter
    {
    public:
        static std::string Header()
        {
            return std::string();
        }

        static std::string Format(const Record& record)
        {
            std::ostringstream oss;
            oss << record.get_message() << "\n";
            return oss.str();
        }
    };
}