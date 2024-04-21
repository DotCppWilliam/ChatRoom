#pragma once

#include "singleton.h"
#include "iappender.h"
#include "record.h"
#include "severity.h"

#include <cassert>
#include <vector>
#include <memory>

namespace logging 
{
    class Logger : public IAppender
    {
    public:
        Logger(Severity max_serverity = none)
            : max_severity_(max_serverity) {}
        
        Logger& AddAppender(std::shared_ptr<IAppender> appender)
        {
            assert(appender.get() != this);
            appenders_.push_back(appender);

            return *this;
        }
        
        /**
         * @brief 单例模式接口,用户对象实例
         * 
         * @return Logger* 
         */
        static Logger* GetInstance()
        {
            return util::Singleton<Logger>::get();
        }

        Severity get_max_severity() const 
        {
            return max_severity_;
        }

        void set_max_severity(Severity severity)
        {
            max_severity_ = severity;
        }

        bool CheckSeverity(Severity severity) const
        {
            return severity <= max_severity_;
        }

        void Write(const Record &record) override 
        {
            if (CheckSeverity(record.get_severity()))
                *this += record;
        }

        void operator+=(const Record& record)
        {
            for (auto it = appenders_.begin(); it != appenders_.end(); ++it)
                (*it)->Write(record);
        }


    private:
        Severity max_severity_;
        std::vector<std::shared_ptr<IAppender>> appenders_;
    };
}