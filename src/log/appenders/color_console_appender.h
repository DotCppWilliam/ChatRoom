#pragma once

#include "console_appender.h"


namespace logging 
{
    template <typename Formatter>
    class ColorConsoleAppender : public ConsoleAppender<Formatter>
    {
    public:
        ColorConsoleAppender(OutputStream out_stream = stream_std_out)
            : ConsoleAppender<Formatter>(out_stream) 
        { this->appender_type_ = color_console_appender; }

        void Write(const Record& record) override
        {
            std::string str = Formatter::Format(record);
            util::AutoLock lock(this->lock_);

            SetColor(record.get_severity());
            this->WriteStr(str);
            ResetColor();
        }

    protected:
        void SetColor(Severity severity)
        {
            if (this->is_atty_)
            {
                switch (severity) 
                {
                    case fatal:     // 粗体 + 红底白字
                        this->output_stream_ << ANSI_BOLD << ANSI_RED_BG << ANSI_WHITE;
                        break;
                    case error:     // 粗体 + 红字
                        this->output_stream_ << ANSI_BOLD << ANSI_RED;
                        break;
                    case warning:    // 黄字
                        this->output_stream_ << ANSI_YELLOW;
                        break;
                    case info:       // 绿字
                        this->output_stream_ << ANSI_GREEN;
                        break;
                    case debug:     // 蓝字
                        this->output_stream_ << ANSI_BLUE;
                        break;
                    case verbose:   // 青色字体
                        this->output_stream_ << ANSI_CYAN;
                        break;
                    default: break;
                }
            }
        }

        void ResetColor()
        {
            if (this->is_atty_)
                this->output_stream_ << ANSI_RESET;
        }
    };
}