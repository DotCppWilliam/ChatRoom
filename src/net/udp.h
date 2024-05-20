#pragma once

#include "buffer.h"
#include "addr.h"
#include "noncopyable.h"
#include "event_base.h"
#include "channel.h"
#include "thread_pool.h"

#include <memory>
#include <functional>


namespace net 
{
    class UdpServer;
    class UdpConn;

    using UdpConnPtr = std::shared_ptr<UdpConn>;
    using UdpServerPtr = std::shared_ptr<UdpServer>;
    using UdpCallBack = std::function<void(const UdpConnPtr &, Buffer)>;
    using UdpSvrCallBack = std::function<void(const UdpServerPtr &, Buffer, Addr)>;


    const int kUdpPacketSize = 4096;
    class UdpServer : public std::enable_shared_from_this<UdpServer>, private util::NonCopyable 
    {
    public:
        UdpServer(EventBases *bases)
            : base_(bases->AllocBase()), bases_(bases), channel_(NULL) {}

        // return 0 on sucess, errno on error
        int Bind(const std::string &host, unsigned short port, bool reusePort = false);
        static UdpServerPtr StartServer(EventBases *bases, const std::string &host, unsigned short port, bool reusePort = false);

        ~UdpServer() { delete channel_; }

        Addr GetAddr() { return addr_; }

        EventBase *GetBase() { return base_; }

        void SendTo(Buffer msg, Addr addr) 
        {
            SendTo(msg.Data(), msg.Size(), addr);
            msg.Clear();
        }
        void SendTo(const char *buf, size_t len, Addr addr);
        void SendTo(const std::string &s, Addr addr) { SendTo(s.data(), s.size(), addr); }
        void SendTo(const char *s, Addr addr) { SendTo(s, strlen(s), addr); }

        //消息的处理
        void OnMsg(const UdpSvrCallBack &callback) { msg_callback_ = callback; }

    private:
        EventBase *base_;
        EventBases *bases_;
        Addr addr_;
        Channel *channel_;
        UdpSvrCallBack msg_callback_;
    };


    // Udp连接，使用引用计数
    class UdpConn : public std::enable_shared_from_this<UdpConn>, private util::NonCopyable  
    {
    public:
        // Udp构造函数，实际可用的连接应当通过createConnection创建
        UdpConn(){};
        virtual ~UdpConn() { Close(); };
        static UdpConnPtr CreateConnection(EventBase *base, const std::string &host, unsigned short port);
        // automatically managed context. allocated when first used, deleted when destruct
        template <class T>
        T &Context() 
        {
            return ctx_.Context<T>();
        }

        EventBase *GetBase() { return base_; }
        Channel *GetChannel() { return channel_; }

        //发送数据
        void Send(Buffer msg) 
        {
            Send(msg.Data(), msg.Size());
            msg.Clear();
        }
        void Send(const char *buf, size_t len);
        void Send(const std::string &s) { Send(s.data(), s.size()); }
        void Send(const char *s) { Send(s, strlen(s)); }
        void OnMsg(const UdpCallBack &cb) { cb_ = cb; }
        void Close();
        //远程地址的字符串
        std::string Str() { return peer_.ToString(); }

    public:
        void HandleRead(const UdpConnPtr &);
        EventBase *base_;
        Channel *channel_;
        Addr local_, peer_;
        AutoContext ctx_;
        std::string destHost_;
        int destPort_;
        UdpCallBack cb_;
    };


    using RetMsgUdpCallBack = std::function<std::string(const UdpServerPtr &, const std::string &, Addr)> ;
    //半同步半异步服务器
    struct HSHAU;

    typedef std::shared_ptr<HSHAU> HSHAUPtr;
    struct HSHAU 
    {
        static HSHAUPtr StartServer(EventBase *base, const std::string &host, unsigned short port, int threads);
        HSHAU(int threads)
        {
            thread_pool_.Start(threads);
        }

        void Exit() 
        {
            thread_pool_.Exit();
        }
        
        void OnMsg(const RetMsgUdpCallBack &cb);
        UdpServerPtr server_;
        ThreadPool thread_pool_;
    };

}