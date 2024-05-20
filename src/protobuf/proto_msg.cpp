#include "proto_msg.h"
#include "log.h"
#include "buffer.h"

#include <google/protobuf/descriptor.h>

using namespace google::protobuf;

void ProtoMsgCodec::Encode(Message *msg, net::Buffer &buf) 
{
    size_t offset = buf.Size();
    buf.AppendVal((uint32_t) 0);
    const std::string &typeName = msg->GetDescriptor()->full_name();
    buf.AppendVal((uint32_t) typeName.size());
    buf.Append(typeName.data(), typeName.size());
    msg->SerializeToArray(buf.AllocRoom(msg->GetCachedSize()), msg->GetCachedSize());
    *(uint32_t *) (buf.Begin() + offset) = buf.Size() - offset;
}


void ProtoMsgDispatcher::Handle(net::TcpConnPtr con, Message *msg) 
{
    auto p = proto_callbacks_.find(msg->GetDescriptor());
    if (p != proto_callbacks_.end()) 
        p->second(con, msg);
    else 
        LOG_FMT_ERROR_MSG("unknown message type %s", msg->GetTypeName().c_str());
}



Message *ProtoMsgCodec::Decode(net::Buffer &s) 
{
    if (s.Size() < 8) 
    {
        LOG_FMT_ERROR_MSG("buffer is too small size: %lu", s.Size());
        return NULL;
    }
    char *p = s.Data();
    uint32_t msglen = *(uint32_t *) p;
    uint32_t namelen = *(uint32_t *) (p + 4);
    if (s.Size() < msglen || s.Size() < 4 + namelen) 
    {
        LOG_FMT_ERROR_MSG("buf format error size %lu msglen %d namelen %d", s.Size(), msglen, namelen);
        return NULL;
    }
    std::string type_name(p + 8, namelen);
    Message *msg = NULL;
    const Descriptor *des = DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
    if (des) 
    {
        const Message *proto = MessageFactory::generated_factory()->GetPrototype(des);
        if (proto) 
            msg = proto->New();
    }
    if (msg == NULL) 
    {
        LOG_FMT_ERROR_MSG("cannot create Message for %s", type_name.c_str());
        return NULL;
    }
    int r = msg->ParseFromArray(p + 8 + namelen, msglen - 8 - namelen);
    if (!r) 
    {
        LOG_ERROR_MSG("bad msg for protobuf");
        delete msg;
        return NULL;
    }
    s.Consume(msglen);
    return msg;
}

inline bool ProtoMsgCodec::MsgComplete(net::Buffer& buf)
{
    return buf.Size() >= 4 && buf.Size() >= *(uint32_t*)buf.Begin();
}