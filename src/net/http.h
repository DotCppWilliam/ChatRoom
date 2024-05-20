#pragma once

#include "buffer.h"
#include "conn.h"

#include <map>

namespace net 
{
    class HttpMsg 
    {
    public:
        enum Result 
        {
            ERROR,
            COMPLETE,
            NOTCOMPLELET,
            CONTINUE100,
        };
        HttpMsg() { HttpMsg::Clear(); };

        std::string GetHeader(const std::string &n) { return GetValueFromMap(headers_, n); }
        Slice GetBody() { return body2_.Size() ? body2_ : (Slice) body_; }

        //如果tryDecode返回Complete，则返回已解析的字节数
        int GetByte() { return scanned_; }

        //内容添加到buf，返回写入的字节数
        virtual int Encode(Buffer &buf) = 0;
        //尝试从buf中解析，默认复制body内容
        virtual Result TryDecode(Slice buf, bool copyBody = true) = 0;
        //清空消息相关的字段
        virtual void Clear();
    public:
        std::map<std::string, std::string> headers_;
        std::string version_, body_;
        // body可能较大，为了避免数据复制，加入body2
        Slice body2_;
    protected:
        bool complete_;
        size_t content_len_;
        size_t scanned_;
        Result TryDecode(Slice buf, bool copyBody, Slice *line1);
        std::string GetValueFromMap(std::map<std::string, std::string> &m, const std::string &n);
    };




    class HttpRequest : public HttpMsg 
    {
    public:
        HttpRequest() { Clear(); }
        std::map<std::string, std::string> args;
        std::string method, uri, query_uri;

        std::string GetArg(const std::string &n) 
        { return GetValueFromMap(args, n); }

        // override
        virtual int Encode(Buffer &buf);
        virtual Result TryDecode(Slice buf, bool copyBody = true);
        virtual void Clear() 
        {
            HttpMsg::Clear();
            args.clear();
            method = "GET";
            query_uri = uri = "";
        }
    };



    struct HttpResponse : public HttpMsg 
    {
        HttpResponse() { Clear(); }
        std::string status_word_;
        int status;
        void SetNotFound() { SetStatus(404, "Not Found"); }
        void SetStatus(int st, const std::string &msg = "") 
        {
            status = st;
            status_word_ = msg;
            body_ = msg;
        }

        // override
        virtual int Encode(Buffer &buf);
        virtual Result TryDecode(Slice buf, bool copyBody = true);
        virtual void Clear() 
        {
            HttpMsg::Clear();
            status = 200;
            status_word_ = "OK";
        }
    };



    // Http连接本质上是一条Tcp连接，下面的封装主要是加入了HttpRequest，HttpResponse的处理
    class HttpConnPtr 
    {
    public:
        HttpConnPtr(const TcpConnPtr &con) : tcp_(con) {}
        operator TcpConnPtr() const { return tcp_; }
        TcpConn *operator->() const { return tcp_.get(); }
        bool operator<(const HttpConnPtr &con) const { return tcp_ < con.tcp_; }

        typedef std::function<void(const HttpConnPtr &)> HttpCallBack;

        HttpRequest &GetRequest() const { return tcp_->internal_ctx_.Context<HttpContext>().req; }
        HttpResponse &GetResponse() const { return tcp_->internal_ctx_.Context<HttpContext>().resp; }

        void SendRequest() const { SendRequest(GetRequest()); }
        void SendResponse() const { SendResponse(GetResponse()); }
        void SendRequest(HttpRequest &req) const 
        {
            req.Encode(tcp_->GetOutput());
            LogOutput("http req");
            ClearData();
            tcp_->SendOutput();
        }
        void SendResponse(HttpResponse &resp) const 
        {
            resp.Encode(tcp_->GetOutput());
            LogOutput("http resp");
            ClearData();
            tcp_->SendOutput();
        }
        //文件作为Response
        void SendFile(const std::string &filename) const;
        void ClearData() const;

        void OnHttpMsg(const HttpCallBack &cb) const;

    protected:
        TcpConnPtr tcp_;

        struct HttpContext 
        {
            HttpRequest req;
            HttpResponse resp;
        };
        void HandleRead(const HttpCallBack &cb) const;
        void LogOutput(const char *title) const;
    };



    using HttpCallBack = HttpConnPtr::HttpCallBack;
    // http服务器
    class HttpServer : public TcpServer 
    {
    public:
        HttpServer(EventBases *base);

        template <class Conn = TcpConn>
        void SetConnType() 
        {
            conn_callback_ = [] { return TcpConnPtr(new Conn); };
        }

        void OnGet(const std::string &uri, const HttpCallBack &cb) 
        { callbacks_["GET"][uri] = cb; }

        void OnRequest(const std::string &method, const std::string &uri, const HttpCallBack &cb) 
        { callbacks_[method][uri] = cb; }

        void OnDefault(const HttpCallBack &callback) { def_callback = callback; }

    private:
        HttpCallBack def_callback;
        std::function<TcpConnPtr()> conn_callback_;
        std::map<std::string, std::map<std::string, HttpCallBack>> callbacks_;
    };

}