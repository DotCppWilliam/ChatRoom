#include "udp.h"
#include "log.h"
#include "net.h"
#include "poller.h"

#include <fcntl.h>
#include <unistd.h>

namespace net 
{
////////////////////////////////////////////////////////////////////// UdpServer
    int UdpServer::Bind(const std::string &host, unsigned short port, bool reuse_port) 
    {
        addr_ = Addr(host, port);
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        int r = SetReuseAddr(fd);
        r = SetReusePort(fd, reuse_port);
        
        r = util::AddFdFlag(fd, FD_CLOEXEC);
        r = ::bind(fd, (struct sockaddr *) &addr_.GetAddr(), sizeof(struct sockaddr));
        if (r) 
        {
            close(fd);
            LOG_FMT_ERROR_MSG("bind to %s failed %d %s", addr_.ToString().c_str(), errno, strerror(errno));
            return errno;
        }
        SetNonBlock(fd);
        LOG_FMT_VERBOSE_MSG("udp fd %d bind to %s", fd, addr_.ToString().c_str());
        channel_ = new Channel(base_, fd, kReadEvent);
        channel_->OnRead([this] 
        {
            Buffer buf;
            struct sockaddr_in raddr;
            socklen_t rsz = sizeof(raddr);
            if (!channel_ || channel_->Fd() < 0) 
            {
                return;
            }
            int fd = channel_->Fd();
            ssize_t rn = recvfrom(fd, buf.MakeRoom(kUdpPacketSize), kUdpPacketSize, 0, (sockaddr *) &raddr, &rsz);
            if (rn < 0) 
            {
                LOG_FMT_ERROR_MSG("udp %d recv failed: %d %s", fd, errno, strerror(errno));
                return;
            }
            buf.AddSize(rn);
            LOG_FMT_VERBOSE_MSG("udp %d recv %ld bytes from %s", fd, rn, Addr(raddr).ToString().data());
            this->msg_callback_(shared_from_this(), buf, raddr);
        });
        return 0;
    }



    UdpServerPtr UdpServer::StartServer(EventBases *bases, const std::string &host, unsigned short port, bool reusePort) 
    {
        UdpServerPtr udp(new UdpServer(bases));
        int r = udp->Bind(host, port, reusePort);
        if (r) 
        {
            LOG_FMT_ERROR_MSG("bind to %s:%d failed %d %s", host.c_str(), port, errno, strerror(errno));
        }
        return r == 0 ? udp : NULL;
    }


    void UdpServer::SendTo(const char *buf, size_t len, Addr addr) 
    {
        if (!channel_ || channel_->Fd() < 0) {
            LOG_FMT_WARNING_MSG("udp sending %lu bytes to %s after channel closed", len, addr.ToString().data());
            return;
        }
        int fd = channel_->Fd();
        int wn = ::sendto(fd, buf, len, 0, (sockaddr *) &addr.GetAddr(), sizeof(sockaddr));
        if (wn < 0) 
        {
            LOG_FMT_ERROR_MSG("udp %d sendto %s error: %d %s", fd, addr.ToString().c_str(), errno, strerror(errno));
            return;
        }
        LOG_FMT_VERBOSE_MSG("udp %d sendto %s %d bytes", fd, addr.ToString().c_str(), wn);
    }







////////////////////////////////////////////////////////////////////// UdpConn
    UdpConnPtr UdpConn::CreateConnection(EventBase *base, const std::string &host, unsigned short port) 
    {
        Addr addr(host, port);
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        
        SetNonBlock(fd);
        int t = util::AddFdFlag(fd, FD_CLOEXEC);
        
        int r = ::connect(fd, (sockaddr *) &addr.GetAddr(), sizeof(sockaddr_in));
        if (r != 0 && errno != EINPROGRESS) 
        {
            LOG_FMT_ERROR_MSG("connect to %s error %d %s", addr.ToString().c_str(), errno, strerror(errno));
            return NULL;
        }
        LOG_FMT_VERBOSE_MSG("udp fd %d connecting to %s ok", fd, addr.ToString().data());
        UdpConnPtr con(new UdpConn);
        con->destHost_ = host;
        con->destPort_ = port;
        con->peer_ = addr;
        con->base_ = base;
        Channel *ch = new Channel(base, fd, kReadEvent);
        con->channel_ = ch;
        ch->OnRead([con] 
        {
            if (!con->channel_ || con->channel_->Fd() < 0) 
                return con->Close();

            Buffer input;
            int fd = con->channel_->Fd();
            int rn = ::read(fd, input.MakeRoom(kUdpPacketSize), kUdpPacketSize);
            if (rn < 0) 
            {
                LOG_FMT_ERROR_MSG("udp read from %d error %d %s", fd, errno, strerror(errno));
                return;
            }

            LOG_FMT_VERBOSE_MSG("udp %d read %d bytes", fd, rn);
            input.AddSize(rn);
            con->cb_(con, input);
        });
        return con;
    }


    void UdpConn::Send(const char *buf, size_t len) 
    {
        if (!channel_ || channel_->Fd() < 0) 
        {
            LOG_FMT_WARNING_MSG("udp sending %lu bytes to %s after channel closed", len, peer_.ToString().data());
            return;
        }
        int fd = channel_->Fd();
        int wn = ::write(fd, buf, len);
        if (wn < 0) 
        {
            LOG_FMT_ERROR_MSG("udp %d write error %d %s", fd, errno, strerror(errno));
            return;
        }
        LOG_FMT_VERBOSE_MSG("udp %d write %d bytes", fd, wn);
    }


    void UdpConn::Close() 
    {
        if (!channel_)
            return;
        auto p = channel_;
        channel_ = NULL;
        base_->SafeCall([p]() { delete p; });
    }


////////////////////////////////////////////////////////////////////// HSHAU
    HSHAUPtr HSHAU::StartServer(EventBase *base, const std::string &host, unsigned short port, int threads) 
    {
        HSHAUPtr p = HSHAUPtr(new HSHAU(threads));
        p->server_ = UdpServer::StartServer(base, host, port);
        return p->server_ ? p : NULL;
    }

    void HSHAU::OnMsg(const RetMsgUdpCallBack &cb) 
    {
        server_->OnMsg([this, cb](const UdpServerPtr &con, Buffer buf, Addr addr) 
        {
            std::string input(buf.Data(), buf.Size());
            thread_pool_.SubmitTask([=]{
                std::string output = cb(con, input, addr);
                server_->GetBase()->SafeCall([=]{
                    if (output.size())
                        con->SendTo(output, addr);
                });
            });
        });
    }
}