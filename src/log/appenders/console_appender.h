#pragma once

#include "iappender.h"
#include "auto_lock.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

namespace logging 
{
    enum OutputStream
    {
        stream_std_out,
        stream_std_err 
    };

    template <typename Formatter>
    class ConsoleAppender : public IAppender
    {
    public:
        ConsoleAppender(OutputStream ostream = stream_std_out)
            : is_atty_(!!isatty(fileno(ostream == stream_std_out ? stdout : stderr))),
              output_stream_(ostream == stream_std_out ? std::cout : std::cerr)
        { this->appender_type_ = console_appender; }

        void Write(const Record& record) override
        {
            std::string str = Formatter::Format(record);
            util::AutoLock lock(this->lock_);

            WriteStr(str);
        }

    protected:
        void WriteStr(const std::string& str) 
        {
            output_stream_ << str << std::flush;
        }
    protected:
        util::Lock      lock_;
        const bool      is_atty_;
        std::ostream&   output_stream_;
    };
}