#pragma once

#include "iappender.h"
#include "auto_lock.h"

#include <unordered_map>

namespace logging 
{
    template <typename Formatter>
    class DynamicAppender : public IAppender
    {
        using Map = std::unordered_map<AppenderType, IAppender*>;
    public:
        DynamicAppender& AddAppender(IAppender* appender)
        {
            util::AutoLock lock(this->lock_);
            appenders_.emplace(appender->get_appender_type(), appender);
            return *this;
        }

        DynamicAppender& RemoveAppender(IAppender* appender)
        {
            util::AutoLock lock(this->lock_);
            auto ret = appenders_.find(appender->get_appender_type());
            if (ret == appenders_.end())
            {
                throw "Unable to delete appender object";
                abort();
            }

            appenders_.erase(ret);
            return *this;
        }

        void Write(const Record& record) override
        {
            util::AutoLock lock(this->lock_);
            for (auto it = appenders_.begin(); it != appenders_.end(); it++)
                (*it).second->Write(record);
        }
    private:
        util::Lock  lock_;
        Map         appenders_;
    };

}