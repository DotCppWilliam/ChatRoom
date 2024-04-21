#pragma once

#include "../record.h"

namespace logging 
{
    enum AppenderType 
    {
        console_appender,
        color_console_appender,
        rolling_file_appender
    };

    class IAppender
    {
    public:
        virtual ~IAppender() {}
        virtual void Write(const Record& record) = 0;
        AppenderType get_appender_type() { return appender_type_; }
    protected:
        AppenderType appender_type_;
    };
}