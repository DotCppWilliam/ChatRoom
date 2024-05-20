#include "state_server.h"
#include "event_base.h"
#include "log.h"
#include "status.h"
#include "file.h"

namespace net 
{
    static std::string QueryLink(const std::string &path) 
    {
        return util::Format("<a href=\"/?stat=%s\">%s</a>", path.c_str(), path.c_str());
    }

    static std::string PageLink(const std::string &path) 
    {
        return util::Format("<a href=\"/%s\">%s</a>", path.c_str(), path.c_str());
    }


    StatServer::StatServer(EventBase *base) 
        : server_(base) 
    {
        server_.OnDefault([this](const HttpConnPtr &con) 
        {
            HttpRequest &req = con.GetRequest();
            HttpResponse &resp = con.GetResponse();
            Buffer buf;
            std::string query = req.GetArg("stat");
            if (query.empty()) 
            {
                query.assign(req.uri.data() + 1, req.uri.size() - 1);
            }

            if (query.size()) 
            {
                auto p = all_callbacks_.find(query); 
                if (p != all_callbacks_.end()) 
                {
                    p->second(req, resp);
                } 
                else 
                {
                    resp.SetNotFound();
                }
            }
            if (req.uri == "/") 
            {
                buf.Append("<a href=\"/\">refresh</a><br/>\n");
                buf.Append("<table>\n");
                buf.Append("<tr><td>Stat</td><td>Desc</td><td>Value</td></tr>\n");
                for (auto &stat : stat_callbacks_) 
                {
                    HttpResponse r;
                    req.uri = stat.first;
                    stat.second.second(req, r);
                    buf.Append("<tr><td>")
                        .Append(PageLink(stat.first))
                        .Append("</td><td>")
                        .Append(stat.second.first)
                        .Append("</td><td>")
                        .Append(r.body_)
                        .Append("</td></tr>\n");
                }
                buf.Append("</table>\n<br/>\n<table>\n").Append("<tr><td>Page</td><td>Desc</td>\n");
                for (auto &stat : page_callbacks_) 
                {
                    buf.Append("<tr><td>").Append(PageLink(stat.first)).Append("</td><td>").Append(stat.second.first).Append("</td></tr>\n");
                }
                buf.Append("</table>\n<br/>\n<table>\n").Append("<tr><td>Cmd</td><td>Desc</td>\n");
                for (auto &stat : cmd_callbacks_) 
                {
                    buf.Append("<tr><td>").Append(QueryLink(stat.first)).Append("</td><td>").Append(stat.second.first).Append("</td></tr>\n");
                }
                buf.Append("</table>\n");
                if (resp.body_.size()) 
                {
                    buf.Append(util::Format("<br/>SubQuery %s:<br/> %s", query.c_str(), resp.body_.c_str()));
                }
                resp.body_ = Slice(buf.Data(), buf.Size());
            }
            LOG_FMT_VERBOSE_MSG("response is: %d \n%.*s", resp.status, (int) resp.body_.size(), resp.body_.data());
            con.SendResponse();
        });
    }


    void StatServer::OnRequest(StatType type, const std::string &key, const std::string &desc, const StatCallBack &cb) 
    {
        if (type == STATE) 
            stat_callbacks_[key] = {desc, cb};
        else if (type == PAGE) 
            page_callbacks_[key] = {desc, cb};
        else if (type == CMD) 
            cmd_callbacks_[key] = {desc, cb};
        else 
        {
            LOG_FMT_ERROR_MSG("unknow state type: %d", type);
            return;
        }
        all_callbacks_[key] = cb;
    }



    void StatServer::OnRequest(StatType type, const std::string &key, const std::string &desc, const InfoCallBack &cb) 
    {
        OnRequest(type, key, desc, [cb](const HttpRequest &, HttpResponse &r) { r.body_ = cb(); });
    }
    

    
    void StatServer::OnPageFile(const std::string &page, const std::string &desc, const std::string &file) 
    {
        return OnRequest(PAGE, page, desc, [file](const HttpRequest &req, HttpResponse &resp) 
        {
            util::Status st = util::File::GetContent(file, resp.body_);
            if (!st.Ok()) 
            {
                LOG_FMT_ERROR_MSG("get file %s failed %s", file.c_str(), st.ToString().c_str());
                resp.SetNotFound();
            } 
            else 
                resp.headers_["Content-Type"] = "text/plain; charset=utf-8";
        });
    }
}