#pragma once

#include "addr.h"
#include "buffer.h"
#include "event_base.h"
#include "noncopyable.h"
#include "channel.h"
#include "codec.h"
#include "event_base.h"
#include "thread_pool.h"

#include <memory>
#include <functional>
#include <list>
#include <unistd.h>
#include <cassert>

namespace net
{
    class TcpConn;
    class TcpServer;

    using TcpConnPtr = std::shared_ptr<TcpConn>;
    using TcpServerPtr = std::shared_ptr<TcpServer>;
    using TcpCallBack = std::function<void(const TcpConnPtr &)>;
    using MsgCallBack = std::function<void(const TcpConnPtr &, Slice msg)>;


    struct IdleNode 
    {
        TcpConnPtr conn_;
        int64_t updated_;
        TcpCallBack callback_;
    };

    struct IdleIdImp 
    {
        using Iter = std::list<IdleNode>::iterator;

        IdleIdImp() {}
        IdleIdImp(std::list<IdleNode> *lst, Iter iter) : list_(lst), iter_(iter) {}
        std::list<IdleNode> *list_;
        Iter iter_;
    };


    class TcpConn : public std::enable_shared_from_this<TcpConn>, util::NonCopyable
    {
        friend class HttpConnPtr;
    public:
        enum State 
        {
            STATTE_INVLAID = 1,
            STATTE_HANDSHAKING, 
            STATTE_CONNECTED, 
            STATTE_CLOSED, 
            STATTE_FAILED, 
        };

        TcpConn();
        virtual ~TcpConn();

        //可传入连接类型，返回智能指针
        template <class C = TcpConn>
        static TcpConnPtr CreateConnection(EventBase *base, const std::string &host, unsigned short port, 
            int timeout = 0, const std::string &localip = "") 
        {
            TcpConnPtr con(new C);
            con->Connect(base, host, port, timeout, localip);
            return con;
        }

        template <class C = TcpConn>
        static TcpConnPtr CreateConnection(EventBase *base, int fd, Addr local, Addr peer) 
        {
            TcpConnPtr conn(new C);
            conn->Attach(base, fd, local, peer);
            
            return conn;
        }

        bool IsClient() { return destPort_ > 0; }

        // 自动管理的上下文。 首次使用时分配，析构时删除
        template <class T>
        T &Context() 
        {
            return ctx_.Context<T>();
        }

        EventBase *GetBase() { return base_; }
        State GetState() { return state_; }
        // TcpConn的输入输出缓冲区
        Buffer &GetInput() { return input_; }
        Buffer &GetOutput() { return output_; }

        Channel *GetChannel() { return channel_; }
        bool Writable() { return channel_ ? channel_->WriteEnabled() : false; }

        //发送数据
        void SendOutput() { Send(output_); }
        void Send(Buffer &msg);
        void Send(const char *buf, size_t len);
        void Send(const std::string &s) { Send(s.data(), s.size()); }
        void Send(const char *s) { Send(s, strlen(s)); }

        //数据到达时回调
        void OnRead(const TcpCallBack &cb) 
        {
            assert(!read_callback_);
            read_callback_ = cb;
        };
        //当tcp缓冲区可写时回调
        void OnWritable(const TcpCallBack &cb) { write_callback_ = cb; }
        // tcp状态改变时回调
        void OnState(const TcpCallBack &cb) { state_callback_ = cb; }
        // tcp空闲回调
        void AddIdleCB(int idle, const TcpCallBack &cb);

        //消息回调，此回调与onRead回调冲突，只能够调用一个
        // codec所有权交给onMsg
        void OnMsg(CodecBase *codec, const MsgCallBack &cb);
        //发送消息
        void SendMsg(Slice msg);

        // conn会在下个事件周期进行处理
        void Close();
        //设置重连时间间隔，-1: 不重连，0:立即重连，其它：等待毫秒数，未设置不重连
        void SetReconnectInterval(int milli) { reconnect_interval_ = milli; }

        //!慎用。立即关闭连接，清理相关资源，可能导致该连接的引用计数变为0，从而使当前调用者引用的连接被析构
        void CloseNow() {
            if (channel_)
                channel_->Close();
        }

        //远程地址的字符串
        std::string Str() { return peer_.ToString(); }
    public:
        void HandleRead(const TcpConnPtr &con);
        void HandleWrite(const TcpConnPtr &con);
        ssize_t Isend(const char *buf, size_t len);
        void Cleanup(const TcpConnPtr &con);
        void Connect(EventBase *base, const std::string &host, unsigned short port, int timeout, const std::string &localip);
        void Reconnect();
        void Attach(EventBase *base, int fd, Addr local, Addr peer);
        virtual int ReadImp(int fd, void *buf, size_t bytes) { return ::read(fd, buf, bytes); }
        virtual int WriteImp(int fd, const void *buf, size_t bytes) { return ::write(fd, buf, bytes); }
        virtual int HandleHandshake(const TcpConnPtr &con);
    private:
        EventBase* base_;
        Channel* channel_;
        Buffer input_;
        Buffer output_;
        Addr local_;
        Addr peer_;
        State state_;
        TcpCallBack read_callback_;
        TcpCallBack write_callback_;
        TcpCallBack state_callback_;
        std::list<IdleId> idle_ids_;
        TimerId timeout_id_;
        AutoContext ctx_;
        AutoContext internal_ctx_;
        std::string destHost_; 
        std::string localIp_;
        int destPort_;
        int connect_timeout_; 
        int reconnect_interval_;
        int64_t connected_time_;
        std::unique_ptr<CodecBase> codec_;
    };






    // Tcp服务器
    struct TcpServer : private util::NonCopyable
    {
        TcpServer(EventBases *bases);
        // return 0 on sucess, errno on error
        int Bind(const std::string &host, unsigned short port, bool reusePort = false);
        static TcpServerPtr StartServer(EventBases *bases, const std::string &host, unsigned short port, bool reusePort = false);
        ~TcpServer() { delete listen_channel_; }

        Addr GetAddr() { return addr_; }
        EventBase *GetBase() { return base_; }
        void OnConnCreate(const std::function<TcpConnPtr()> &cb) { createcb_ = cb; }
        void OnConnState(const TcpCallBack &cb) { statecb_ = cb; }
        void OnConnRead(const TcpCallBack &cb) 
        {
            readcb_ = cb;
            assert(!msgcb_);
        }
        // 消息处理与Read回调冲突，只能调用一个
        void OnConnMsg(CodecBase *codec, const MsgCallBack &cb) 
        {
            codec_.reset(codec);
            msgcb_ = cb;
            assert(!readcb_);
        }

    private:
        EventBase *base_;
        EventBases *bases_;
        Addr addr_;
        Channel *listen_channel_;
        TcpCallBack statecb_, readcb_;
        MsgCallBack msgcb_;
        std::function<TcpConnPtr()> createcb_;
        std::unique_ptr<CodecBase> codec_;
        void handleAccept();
    };






    typedef std::function<std::string(const TcpConnPtr &, const std::string &msg)> RetMsgCallBack;
    //半同步半异步服务器
    struct HSHA;
    typedef std::shared_ptr<HSHA> HSHAPtr;
    struct HSHA 
    {
        static HSHAPtr StartServer(EventBase *base, const std::string &host, unsigned short port, int threads);
        HSHA(int threads)  
        {
            thread_pool_.Start(threads);
        }
        
        void Exit() 
        {
            thread_pool_.Exit();
        }
        void OnMsg(CodecBase *codec, const RetMsgCallBack &cb);
        TcpServerPtr server_;
        ThreadPool thread_pool_;
    };
}