#pragma once

#include "noncopyable.h"
#include "event_base.h"
#include "http.h"

#include <map>

namespace net 
{
    using StatCallBack = std::function<void(const HttpRequest&, HttpResponse&)>;
    using InfoCallBack = std::function<std::string()>;
    using IntCallBack = std::function<int64_t()>;

    class StatServer : private util::NonCopyable 
    {
    public:
        enum StatType 
        {
            STATE,
            PAGE,
            CMD,
        };

        StatServer(EventBase *base);
        int Bind(const std::string &host, unsigned short port) 
        { return server_.Bind(host, port); }

        void OnRequest(StatType type, const std::string &key, const std::string &desc, const StatCallBack &cb);
        void OnRequest(StatType type, const std::string &key, const std::string &desc, const InfoCallBack &cb);
        //用于展示一个简单的state
        void OnState(const std::string &state, const std::string &desc, const InfoCallBack &cb) 
        { OnRequest(STATE, state, desc, cb); }
        
        void OnState(const std::string &state, const std::string &desc, const IntCallBack &cb) 
        {
            OnRequest(STATE, state, desc, [cb] { 
                return util::Format("%ld", cb()); 
            });
        }
        //用于展示一个页面
        void OnPage(const std::string &page, const std::string &desc, const InfoCallBack &cb) 
        { OnRequest(PAGE, page, desc, cb); }

        void OnPageFile(const std::string &page, const std::string &desc, const std::string &file);
        //用于发送一个命令
        void onCmd(const std::string &cmd, const std::string &desc, const InfoCallBack &cb) 
        { OnRequest(CMD, cmd, desc, cb); }

        void OnCmd(const std::string &cmd, const std::string &desc, const IntCallBack &cb) 
        {
            OnRequest(CMD, cmd, desc, [cb] { 
                return util::Format("%ld", cb()); 
            });
        }

    private:
        HttpServer server_;
        using DescState = std::pair<std::string, StatCallBack>;
        std::map<std::string, DescState> stat_callbacks_, page_callbacks_, cmd_callbacks_;
        std::map<std::string, StatCallBack> all_callbacks_;
    };

}