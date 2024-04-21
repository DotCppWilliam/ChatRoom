#pragma once

#include "record.h"
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>



namespace logging 
{
    template <bool UseUtcTime>
    class TxtFormatterImpl
    {
    public:
        static std::string Header()
        {
            return std::string();
        }

        static std::string Format(const Record& record)
        {
            tm t;
            UseUtcTime ? util::GmTime_s(&t, &record.get_time().time)
                    : util::LocalTime_s(&t, &record.get_time().time);
            std::ostringstream oss;
        // 日志级别
            oss << "[" << SeverityToString(record.get_severity()) << "]" << " ";
        // 时间
            oss << t.tm_year + 1900 << "-" << std::setfill('0') << std::setw(2)
                << t.tm_mon + 1 << "-" << std::setfill('0') << std::setw(2)
                << t.tm_mday << " ";
        
            oss << std::setfill('0') << std::setw(2)
                << t.tm_hour << ":" << std::setfill('0') << std::setw(2)
                << t.tm_min << ":" << std::setfill('0') << std::setw(2)
                << t.tm_sec << "." << std::setfill('0') << std::setw(3)
                << static_cast<int>(record.get_time().time) << " ";
        
            oss << std::setfill(' ') << std::setw(5) << std::left << " ";
        // 线程id
            oss << "[" << record.get_tid() << "] ";
        // 文件名+行号
            oss << "[" << record.get_file() << " --- line:" << record.get_line() << "] ";
        // 日志消息
            oss << record.get_message() << "\n";

            return oss.str();
        }
    };

    class TxtFormatter : public TxtFormatterImpl<false> {};
    class TxtFormatterUtcTime : public TxtFormatterImpl<true> {};
}