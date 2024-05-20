#include "codec.h"
#include "buffer.h"
#include "slice.h"
#include "codec.h"
#include "net.h"


#include <cstdint>
#include <cstring>

namespace net 
{
/////////////////////////////////////////////////////// LineCodec
    int LineCodec::TryDecode(Slice data, Slice& msg)
    {
        if (data.Size() == 1 && data[0] == 0x04)
        {
            msg = data;
            return 1;
        }

        for (size_t i = 0; i < data.Size(); i++)
        {
            if (data[i] == '\n')
            {
                if (i > 0 && data[i - 1] == '\r')
                {
                    msg = Slice(data.Data(), i - 1);
                    return static_cast<int>(i + 1);
                }
                else 
                {
                    msg = Slice(data.Data(), i);
                    return static_cast<int>(i + 1);
                }
            }
        }
        return 0;
    }


    void LineCodec::Encode(Slice msg, Buffer& buf)
    {
        buf.Append(msg).Append("\r\n");
    }

    CodecBase* LineCodec::Clone() 
    { return new LineCodec(); }


/////////////////////////////////////////////////////// LengthCodec
    int LengthCodec::TryDecode(Slice data, Slice& msg)
    {
        if (data.Size() < 8)
            return 0;
        
        int len = ntoh(*reinterpret_cast<int32_t*>(data.Data() + 4));
        if (len > 1024 * 1024 || memcmp(data.Data(), "mBdT", 4) != 0)
            return -1;

        if (static_cast<int>(data.Size()) >= len + 8)
        {
            msg = Slice(data.Data() + 8, len);
            return len + 8;
        }

        return 0;
    }

    void LengthCodec::Encode(Slice msg, Buffer& buf)
    {
        buf.Append("mBdT").AppendVal(
            hton(static_cast<int32_t>(msg.Size()))).Append(msg);
    }

    CodecBase* LengthCodec::Clone()
    {
        return new LengthCodec();
    }
}