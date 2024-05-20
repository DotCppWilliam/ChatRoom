#pragma once

#include "conn.h"

#include <functional>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <unordered_map>


using Message = ::google::protobuf::Message;
using Descriptor = ::google::protobuf::Descriptor;
using ProtoCallBack = std::function<void(net::TcpConnPtr conn, Message* msg)>;

struct ProtoMsgCodec
{
    static void Encode(Message* msg, net::Buffer& buf);
    static Message* Decode(net::Buffer& buf);
    static bool MsgComplete(net::Buffer& buf);
};


struct ProtoMsgDispatcher
{
    void Handle(net::TcpConnPtr conn, Message* msg);
    
    template <typename T>
    void onMsg(std::function<void(net::TcpConnPtr conn, T* msg)> callback)
    {
        proto_callbacks_[T::Descriptor()] = [callback](net::TcpConnPtr conn, Message* msg)
        {
            callback(conn, dynamic_cast<T*>(msg));
        };
    }
private:
    std::unordered_map<const Descriptor*, ProtoCallBack> proto_callbacks_;
};



