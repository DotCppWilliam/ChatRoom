#pragma once

#include "buffer.h"
#include "slice.h"

namespace net 
{
    struct CodecBase 
    {
        /**
         * @brief 解析消息,消息放在msg中,返回已扫描的字节数
         * 
         * @param data 
         * @param msg 
         * @return int 大于0: 解析出完整消息. 等于0: 解析部分消息; 小于0: 解析错误
         */
        virtual int TryDecode(Slice data, Slice& msg) = 0;
        virtual void Encode(Slice msg, Buffer& buf) = 0;
        virtual CodecBase* Clone() = 0;
        virtual ~CodecBase() = default;
    };

    // 解析 \r\n结尾的消息
    struct LineCodec : public CodecBase
    {
        int TryDecode(Slice data, Slice& msg) override;
        void Encode(Slice msg, Buffer& buf) override;
        CodecBase* Clone() override;
    };

    // 解析出长度
    struct LengthCodec : public CodecBase
    {
        int TryDecode(Slice data, Slice& msg) override;
        void Encode(Slice msg, Buffer& buf) override;
        CodecBase* Clone() override;
    };
}