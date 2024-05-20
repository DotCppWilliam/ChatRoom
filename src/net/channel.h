#pragma once

#include "noncopyable.h"
#include "event_base.h"
#include <functional>

namespace net 
{
    class PollerBase;

    class Channel : private util::NonCopyable
    {
        using Task = std::function<void()>;
    public:
        // base为事件管理器，fd为通道内部的fd，events为通道关心的事件
        Channel(EventBase *base, int fd, int events);
        ~Channel();
        EventBase *GetBase() { return base_; }
        int Fd() { return fd_; }
        //通道id
        int64_t Id() { return id_; }
        short Events() { return events_; }
        //关闭通道
        void Close();

        //挂接事件处理器
        void OnRead(const Task &readcb) { read_callback_ = readcb; }
        void OnWrite(const Task &writecb) { write_callback_ = writecb; }
        void OnRead(Task &&readcb) { read_callback_ = std::move(readcb); }
        void OnWrite(Task &&writecb) { write_callback_ = std::move(writecb); }

        //启用读写监听
        void EnableRead(bool enable);
        void EnableWrite(bool enable);
        void EnableReadWrite(bool readable, bool writable);
        bool ReadEnabled();
        bool WriteEnabled();

        //处理读写事件
        void HandleRead() { read_callback_(); }
        void HandleWrite() { write_callback_(); }

    protected:
        EventBase *base_;
        PollerBase *poller_;
        int fd_;
        short events_;
        int64_t id_;
        Task read_callback_;
        Task write_callback_;
        Task error_callback_;
    };
}