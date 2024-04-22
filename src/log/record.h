#pragma once

#include "severity.h"
#include "file.h"
#include "timestamp.h"
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <sstream>
#include <syscall.h>
#include <stdarg.h>

namespace logging 
{
    class Record 
    {
    public:
        Record()
            : func_(__FUNCTION__), line_(__LINE__), 
            file_(__FILE__), severity_(logging::Severity::debug),
            tid_(::syscall(SYS_gettid)), object_(nullptr) {}

        Record(Severity severity, const char* func, size_t line,
            const char* file, const void* object = nullptr)
            : severity_(severity), tid_(::syscall(SYS_gettid)),
            object_(object), line_(line), func_(func), file_(file) 
        { util::Ftime(&time_); }


        // Record(Severity severity, const char* message) 
        //     : logging::Record()
        // { 
        //     severity_ = severity;
        //     message_ << message; 
        // }

        Record(Severity severity, const char* format, ...) 
            : logging::Record()
        { 
            severity_ = severity;
            va_list ap;

            va_start(ap, format);
            Format(format, ap);
            va_end(ap);
        }

        Record(const char* message) : logging::Record()
        { message_ << message; }
        
        Record(const char* format, ...) : logging::Record()
        {
            va_list ap;

            va_start(ap, format);
            Format(format, ap);
            va_end(ap);
        }

        Record& Ref()
        {
            return *this;
        }

        Record& operator<<(char data)
        {
            char str[] = { data, 0 };
            return *this << str;
        }

        Record& operator<<(std::ostream& (*data) (std::ostream&))
        {
            message_ << data;
            return *this;
        }

        template <typename T>
        Record& operator<<(const T& data)
        {
            message_ << data;
            return *this;
        }

        Record& Printf(const char* format, ...)
        {
            va_list ap;

            va_start(ap, format);
            Format(format, ap);
            va_end(ap);

            return *this;
        }


        virtual ~Record() {}

        virtual Severity get_severity() const
        {
            return severity_;
        }

        virtual const util::Time& get_time() const 
        {
            return time_;
        }

        virtual unsigned int get_tid() const 
        {
            return tid_;
        }

        virtual size_t get_line() const 
        {
            return line_;
        }

        virtual const char* get_message() const 
        {
            message_str_ = message_.str();
            return message_str_.c_str();
        }

        virtual const char* get_func() const 
        {
            func_str_ = util::ProcessFuncName(func_);
            return func_str_.c_str();
        }

        virtual const char* get_file() const 
        {
            return file_;
        }

        virtual const void* get_object() const 
        {
            return object_;
        }
    protected:
        void Format(const char* format, va_list args)
        {
            char* str = nullptr;
            int len = vasprintf(&str, format, args);
            static_cast<void>(len);

            *this << str;
            free(str);
        }
    private:
        Severity                severity_;
        util::Time              time_;
        const unsigned int      tid_;
        const void* const       object_;
        const size_t            line_;
        std::ostringstream      message_;
        const char* const       func_;
        const char* const       file_;
        mutable std::string     func_str_;
        mutable std::string     message_str_;
    };
}