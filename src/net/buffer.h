#pragma once

#include "slice.h"
#include <cstddef>

namespace net 
{
    class Buffer 
    {
    public:
        Buffer() : buf_(nullptr), beg_(0), end_(0), capacity_(0), expand_(512) {}
        ~Buffer() { delete [] buf_; }

        Buffer(const Buffer& buf);
        Buffer& operator=(const Buffer& buf);
        operator Slice();
    public:
        size_t Size() const { return end_ - beg_; }
        bool Empty() const { return end_ == beg_; }
        char* Data() const { return buf_ + beg_; }
        char* Begin() const { return buf_ + beg_; }
        char* End() const { return buf_ + end_; }
        size_t Space() const { return capacity_ - end_; }
        void AddSize(size_t len) { end_ += len; }

        void Clear();
        char* MakeRoom(size_t len);
        void MakeRoom();
        char* AllocRoom(size_t len);

        Buffer& Append(const char* str);
        Buffer& Append(const char* str, size_t len);
        Buffer& Append(Slice slice);

        template <typename T>
        Buffer& AppendVal(const T& val)
        {
            Append(reinterpret_cast<const char*>(&val), sizeof(T));
            return *this;
        }

        Buffer& Consume(size_t len);
        Buffer& Absorb(Buffer& buf);
        void SetSuggestSize(size_t size);
        

    private:
        void MoveHead();
        void Expand(size_t len);
        void CopyFrom(const Buffer& buf);
    private:
        char* buf_;
        size_t beg_;
        size_t end_;
        size_t capacity_;
        size_t expand_;    // 未知什么含义
    };
}