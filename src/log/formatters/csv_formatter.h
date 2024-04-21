#pragma once

#include "record.h"
#include "timestamp.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <ctime>

namespace logging 
{
    template <bool UseUtcTime>
    class CsvFormatterImpl
    {
    public:
        static std::string Header()
        {
            return "Date;Time;Severity;TID;This;Function;Message\n";
        }

        static std::string Format(const Record& record)
        {
            tm t;
            UseUtcTime ? util::GmTime_s(&t, &record.get_time().time) 
                : util::LocalTime_s(&t, &record.get_time().time);

            std::ostringstream oss;
            oss << t.tm_year + 1900 << "/" << std::setfill('0') << std::setw(2) 
                << t.tm_mon + 1 << "/" << std::setfill('0') << std::setw(2)
                << t.tm_mday << ";";
            
            oss << std::setfill('0') << std::setw(2) << t.tm_hour << ":"
                    << std::setfill('0') << std::setw(2) 
                << t.tm_min << ":" << std::setfill('0') << std::setw(2) 
                << t.tm_sec << "." << std::setfill('0') << std::setw(3)
                << static_cast<int>(record.get_time().millisecond) << ";";

            oss << SeverityToString(record.get_severity()) << ";";
            oss << record.get_tid() << ";";
            oss << record.get_object() << ";";
            oss << record.get_func() << "@" << record.get_line() << ";";
            
            std::string message = record.get_message();
            if (message.size() > kMaxMessageSize)
            {
                message.resize(kMaxMessageSize);
                message.append("...");
            }

            std::istringstream split(message);
            std::string token;

            while (!split.eof())
            {
                std::getline(split, token, '"');
                oss << "\"" << token << "\"";
            }
            oss << "\n";

            return oss.str();
        }

        static const size_t kMaxMessageSize = 32000;
    };

    class CsvFormatter : public CsvFormatterImpl<false> {};
    class CsvFormatterUtcTime : public CsvFormatterImpl<true> {};
}