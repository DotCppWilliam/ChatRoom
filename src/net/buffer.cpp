#include "buffer.h"
#include "slice.h"
#include <algorithm>

namespace net 
{
    Buffer& Buffer::operator=(const Buffer& buf)
    {
        if (&buf == this)
            return *this;

        delete [] buf_;
        buf_ = nullptr;
        CopyFrom(buf);

        return *this;
    }

    Buffer::operator Slice() 
    { return Slice(Data(), Size()); }

    void Buffer::Clear()
    {
        delete [] buf_;
        buf_ = nullptr;
        capacity_ = 0;
        beg_ = end_ = 0;
    }

    char* Buffer::MakeRoom(size_t len)
    {
        if (Size() + len < capacity_ / 2)
            MoveHead();
        else
            Expand(len);

        return End();
    }

    void Buffer::MakeRoom()
    {
        if (Space() < expand_)
            Expand(0);
    }

    char* Buffer::AllocRoom(size_t len)
    {
        char* ptr = MakeRoom(len);
        AddSize(len);
        return ptr;
    }

    Buffer& Buffer::Append(const char* str, size_t len)
    {
        memcpy(AllocRoom(len), str, len);

        return *this;
    }

    Buffer& Buffer::Append(const char* str)
    {
        return Append(str, strlen(str));
    }

    Buffer& Buffer::Append(Slice slice)
    {
        return Append(slice.Data(), slice.Size());
    }

    Buffer& Buffer::Consume(size_t len)
    {
        beg_ += len;
        if (Size() == 0)
            Clear();

        return *this;
    }

    Buffer& Buffer::Absorb(Buffer& buf)
    {
        if (&buf != this)
        {
            if (Size() == 0)
            {
                char buf_arr[sizeof(buf)];
                memcpy(buf_arr, this, sizeof(buf));
                memcpy(this, &buf, sizeof(buf_arr));
                memcpy(&buf, buf_arr, sizeof(buf_arr));
                std::swap(expand_, buf.expand_);
            }
            else 
            {
                Append(buf.Begin(), buf.Size());
                buf.Clear();
            }
        }
        return *this;
    }


    void Buffer::SetSuggestSize(size_t size)
    {
        expand_ = size;
    }

    void Buffer::MoveHead()
    {
        std::copy(Begin(), End(), buf_);
        end_ -= beg_;
        beg_ = 0;
    }

    void Buffer::Expand(size_t len)
    {
        size_t capacity = std::max(expand_, std::max(2 * capacity_, Size() + len));
        char* ptr = new char[capacity];
        std::copy(Begin(), End(), ptr);

        end_ -= beg_;
        beg_ = 0;
        delete [] buf_;
        buf_ = ptr;
        capacity_ = capacity;
    }

    void Buffer::CopyFrom(const Buffer& buf)
    {
        memcpy(this, &buf, sizeof(buf));
        if (buf.buf_)
        {
            buf_ = new char[capacity_];
            memcpy(Data(), buf.Begin(), buf.Size());
        }
    }
}