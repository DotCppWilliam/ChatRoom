#include "conn.h"
#include "event_base.h"
#include "log.h"
#include "poller.h"
#include "slice.h"
#include "util.h"
#include "net.h"
#include "thread_pool.h"

namespace net 
{
    using namespace std;
    void HandyUnregisterIdle(EventBase *base, const IdleId &idle);
    void HandyUpdateIdle(EventBase *base, const IdleId &idle);

    void TcpConn::Attach(EventBase *base, int fd, Addr local, Addr peer) 
    {
        base_ = base;
        state_ = State::STATTE_HANDSHAKING;
        local_ = local;
        peer_ = peer;
        delete channel_;
        channel_ = new Channel(base, fd, kWriteEvent | kReadEvent);
        LOG_FMT_VERBOSE_MSG("Tcp constructed %s - %s fd: %d\n", local.ToString().c_str(), 
            peer_.ToString().c_str(), fd);


        TcpConnPtr conn = shared_from_this();
        conn->channel_->OnRead([=] { conn->HandleRead(conn); });
        conn->channel_->OnWrite([=] { conn->HandleWrite(conn); });
    }


    void TcpConn::Connect(EventBase *base, const string &host, 
        unsigned short port, int timeout, const string &localip) 
    {
        destHost_ = host;
        destPort_ = port;
        connect_timeout_ = timeout;
        connected_time_ = util::TimeMilli();
        localIp_ = localip;
        Addr addr(host, port);

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        SetNonBlock(fd);
        int t = util::AddFdFlag(fd, FD_CLOEXEC);
        
        int ret = 0;
        if (localip.size()) 
        {
            Addr addr(localip, 0);
            ret = ::bind(fd, (struct sockaddr *) &addr.GetAddr(), sizeof(struct sockaddr));
            LOG_FMT_ERROR_MSG("bind to %s failed LOG_FMT_ERROR_MSG %d %s", addr.ToString().c_str(), 
                errno, strerror(errno));
        }
        if (ret == 0) 
        {
            ret = ::connect(fd, (sockaddr *) &addr.GetAddr(), sizeof(sockaddr_in));
            if (ret != 0 && errno != EINPROGRESS) 
                LOG_FMT_ERROR_MSG("connect to %s LOG_FMT_ERROR_MSG %d %s", addr.ToString().c_str(), 
                    errno, strerror(errno));
            
        }

        sockaddr_in local;
        socklen_t alen = sizeof(local);
        if (ret == 0) 
        {
            ret = getsockname(fd, (sockaddr *) &local, &alen);
            if (ret < 0) {
                LOG_FMT_ERROR_MSG("getsockname failed %d %s", errno, strerror(errno));
            }
        }
        state_ = State::STATTE_HANDSHAKING;
        Attach(base, fd, Addr(local), addr);
        if (timeout) 
        {
            TcpConnPtr conn = shared_from_this();
            timeout_id_ = base->RunAfter(timeout, [conn] {
                if (conn->GetState() == STATTE_HANDSHAKING) 
                    conn->channel_->Close();
            });
        }
    }

    void TcpConn::Close() 
    {
        if (channel_) 
        {
            TcpConnPtr conn = shared_from_this();
            GetBase()->SafeCall([conn] {
                if (conn->channel_)
                    conn->channel_->Close();
            });
        }
    }

    void TcpConn::Cleanup(const TcpConnPtr &conn) 
    {
        if (read_callback_ && input_.Size()) 
            read_callback_(conn);
        
        if (state_ == State::STATTE_HANDSHAKING) 
            state_ = State::STATTE_FAILED;
        else 
            state_ = State::STATTE_CLOSED;
        
        LOG_FMT_VERBOSE_MSG("tcp closing %s - %s fd %d %d", local_.ToString().c_str(), 
            peer_.ToString().c_str(), channel_ ? channel_->Fd() : -1, errno);
        GetBase()->Cancel(timeout_id_);
        if (state_callback_) {
            state_callback_(conn);
        }
        if (reconnect_interval_ >= 0 && !GetBase()->Exited()) 
        {  
            Reconnect();
            return;
        }
        for (auto &idle : idle_ids_) 
        {
            HandyUnregisterIdle(GetBase(), idle);
        }

        // channel may have hold TcpConnPtr, set channel_ to NULL before delete
        read_callback_ = write_callback_ = state_callback_ = nullptr;
        Channel *ch = channel_;
        channel_ = NULL;
        delete ch;
    }


    void TcpConn::HandleRead(const TcpConnPtr &con) 
    {
        if (state_ == State::STATTE_HANDSHAKING && HandleHandshake(con)) {
            return;
        }
        while (state_ == State::STATTE_CONNECTED) 
        {
            input_.MakeRoom();
            int rd = 0;
            if (channel_->Fd() >= 0) 
            {
                rd = ReadImp(channel_->Fd(), input_.End(), input_.Space());
                LOG_FMT_VERBOSE_MSG("channel %lld fd %d readed %d bytes", 
                    (long long)channel_->Id(), channel_->Fd(), rd);
            }
            if (rd == -1 && errno == EINTR) 
            {
                continue;
            } 
            else if (rd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) 
            {
                for (auto &idle : idle_ids_) 
                    HandyUpdateIdle(GetBase(), idle);
                
                if (read_callback_ && input_.Size()) 
                    read_callback_(con);
                
                break;
            }
            else if (channel_->Fd() == -1 || rd == 0 || rd == -1) 
            {
                Cleanup(con);
                break;
            } 
            else 
                input_.AddSize(rd);
        }
    }


    int TcpConn::HandleHandshake(const TcpConnPtr &con) 
    {
        struct pollfd pfd;
        pfd.fd = channel_->Fd();
        pfd.events = POLLOUT | POLLERR;
        int r = poll(&pfd, 1, 0);
        if (r == 1 && pfd.revents == POLLOUT) {
            channel_->EnableReadWrite(true, false);
            state_ = State::STATTE_CONNECTED;
            if (state_ == State::STATTE_CONNECTED) 
            {
                connected_time_ = util::TimeMilli();
                LOG_FMT_VERBOSE_MSG("tcp connected %s - %s fd %d", 
                    local_.ToString().c_str(), peer_.ToString().c_str(), channel_->Fd());
                if (state_callback_) 
                    state_callback_(con);
            }
        } 
        else 
        {
            LOG_FMT_VERBOSE_MSG("poll fd %d return %d revents %d", channel_->Fd(), r, 
                pfd.revents);
            Cleanup(con);
            return -1;
        }
        return 0;
    }


    void TcpConn::HandleWrite(const TcpConnPtr &conn) 
    {
        if (state_ == State::STATTE_HANDSHAKING) 
        {
            HandleHandshake(conn);
        } 
        else if (state_ == State::STATTE_CONNECTED) 
        {
            ssize_t sended = Isend(output_.Begin(), output_.Size());
            output_.Consume(sended);
            if (output_.Empty() && write_callback_) 
            {
                write_callback_(conn);
            }
            if (output_.Empty() && channel_->WriteEnabled()) 
            {  // writablecb_ may write something
                channel_->EnableWrite(false);
            }
        } 
        else 
        {
            LOG_ERROR_MSG("handle write unexpected");
        }
    }


    ssize_t TcpConn::Isend(const char *buf, size_t len) 
    {
        size_t sended = 0;
        while (len > sended) 
        {
            ssize_t wd = WriteImp(channel_->Fd(), buf + sended, len - sended);
            LOG_FMT_VERBOSE_MSG("channel %lld fd %d write %ld bytes", (long long) channel_->Id(), 
                channel_->Fd(), wd);
            if (wd > 0) 
            {
                sended += wd;
                continue;
            } 
            else if (wd == -1 && errno == EINTR) 
            {
                continue;
            } 
            else if (wd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) 
            {
                if (!channel_->WriteEnabled()) 
                    channel_->EnableWrite(true);
                
                break;
            } 
            else 
            {
                LOG_FMT_ERROR_MSG("write error: channel %lld fd %d wd %ld %d %s", (long long) channel_->Id(), 
                    channel_->Fd(), wd, errno, strerror(errno));
                break;
            }
        }
        return sended;
    }

    void TcpConn::Send(Buffer &buf) 
    {
        if (channel_) 
        {
            if (channel_->WriteEnabled()) 
                output_.Absorb(buf);
            
            if (buf.Size()) 
            {
                ssize_t sended = Isend(buf.Begin(), buf.Size());
                buf.Consume(sended);
            }
            if (buf.Size()) 
            {
                output_.Absorb(buf);
                if (!channel_->WriteEnabled()) 
                    channel_->EnableWrite(true);
                
            }
        } 
        else 
        {
            LOG_FMT_WARNING_MSG("connection %s - %s closed, but still writing %lu bytes", 
                local_.ToString().c_str(), peer_.ToString().c_str(), buf.Size());
        }
    }

    void TcpConn::Send(const char *buf, size_t len) 
    {
        if (channel_) 
        {
            if (output_.Empty()) 
            {
                ssize_t sended = Isend(buf, len);
                buf += sended;
                len -= sended;
            }
            if (len)
                output_.Append(buf, len);
            
        } 
        else 
        {
            LOG_FMT_WARNING_MSG("connection %s - %s closed, but still writing %lu bytes", 
                local_.ToString().c_str(), peer_.ToString().c_str(), len);
        }
    }

    void TcpConn::OnMsg(CodecBase *codec, const MsgCallBack &cb) 
    {
        codec_.reset(codec);
        OnRead([cb](const TcpConnPtr &con) {
            int r = 1;
            while (r) 
            {
                Slice msg;
                r = con->codec_->TryDecode(con->GetInput(), msg);
                if (r < 0) 
                {
                    con->channel_->Close();
                    break;
                } 
                else if (r > 0) 
                {
                    LOG_FMT_VERBOSE_MSG("a msg decoded. origin len %d msg len %ld", r, msg.Size());
                    cb(con, msg);
                    con->GetInput().Consume(r);
                }
            }
        });
    }

    void TcpConn::SendMsg(Slice msg) 
    {
        codec_->Encode(msg, GetOutput());
        SendOutput();
    }

    TcpServer::TcpServer(EventBases *bases) 
        : base_(bases->AllocBase()), bases_(bases), listen_channel_(NULL), 
        createcb_([] { return TcpConnPtr(new TcpConn); }) {}

    int TcpServer::Bind(const std::string &host, unsigned short port, bool reusePort) 
    {
        addr_ = Addr(host, port);
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        int r = net::SetReuseAddr(fd);
        r = net::SetReusePort(fd, reusePort);
        
        r = util::AddFdFlag(fd, FD_CLOEXEC);
        
        r = ::bind(fd, (struct sockaddr *) &addr_.GetAddr(), sizeof(struct sockaddr));
        if (r) 
        {
            close(fd);
            LOG_FMT_ERROR_MSG("bind to %s failed %d %s", addr_.ToString().c_str(), errno, strerror(errno));
            return errno;
        }
        r = listen(fd, 20);
        
        LOG_FMT_INFO_MSG("fd %d listening at %s", fd, addr_.ToString().c_str());
        listen_channel_ = new Channel(base_, fd, kReadEvent);
        listen_channel_->OnRead([this] { handleAccept(); });
        return 0;
    }

    TcpServerPtr TcpServer::StartServer(EventBases *bases, const std::string &host, 
        unsigned short port, bool reusePort) 
    {
        TcpServerPtr p(new TcpServer(bases));
        int r = p->Bind(host, port, reusePort);
        if (r) 
            LOG_FMT_ERROR_MSG("bind to %s:%d failed %d %s", host.c_str(), port, errno, strerror(errno));
        
        return r == 0 ? p : NULL;
    }

    void TcpServer::handleAccept() 
    {
        struct sockaddr_in raddr;
        socklen_t rsz = sizeof(raddr);
        int lfd = listen_channel_->Fd();
        int cfd;
        while (lfd >= 0 && (cfd = accept(lfd, (struct sockaddr *) &raddr, &rsz)) >= 0) 
        {
            sockaddr_in peer, local;
            socklen_t alen = sizeof(peer);
            int r = getpeername(cfd, (sockaddr *) &peer, &alen);
            if (r < 0) 
            {
                LOG_FMT_ERROR_MSG("get peer name failed %d %s", errno, strerror(errno));
                continue;
            }
            r = getsockname(cfd, (sockaddr *) &local, &alen);
            if (r < 0) {
                LOG_FMT_ERROR_MSG("getsockname failed %d %s", errno, strerror(errno));
                continue;
            }
            r = util::AddFdFlag(cfd, FD_CLOEXEC);
            
            EventBase *b = bases_->AllocBase();
            auto addcon = [=] 
            {
                TcpConnPtr con = createcb_();
                con->Attach(b, cfd, local, peer);
                if (statecb_) {
                    con->OnState(statecb_);
                }
                if (readcb_) {
                    con->OnRead(readcb_);
                }
                if (msgcb_) {
                    con->OnMsg(codec_->Clone(), msgcb_);
                }
            };
            if (b == base_) 
                addcon();
            else 
                b->SafeCall(std::move(addcon));
            
        }
        if (lfd >= 0 && errno != EAGAIN && errno != EINTR) {
            LOG_FMT_WARNING_MSG("accept return %d  %d %s", cfd, errno, strerror(errno));
        }
    }

    HSHAPtr HSHA::StartServer(EventBase *base, const std::string &host, unsigned short port, int threads) 
    {
        HSHAPtr p = HSHAPtr(new HSHA(threads));
        p->server_ = TcpServer::StartServer(base, host, port);
        return p->server_ ? p : NULL;
    }


    void HSHA::OnMsg(CodecBase *codec, const RetMsgCallBack &cb) 
    {
        server_->OnConnMsg(codec, [this, cb](const TcpConnPtr& conn, Slice msg){
            std::string input = msg;
            thread_pool_.SubmitTask([=]{
                std::string output = cb(conn, input);
                server_->GetBase()->SafeCall([=]{
                    if (output.size())
                        conn->SendMsg(output);
                });
            });
        });
    }
}