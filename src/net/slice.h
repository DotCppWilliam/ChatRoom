#pragma once

#include <string>
#include <cstring>
#include <vector>

namespace net 
{
    class Slice 
    {
    public: 
        Slice() : buf_end_(""), buf_beg_("") {}
        Slice(const char* beg, const char* end) : buf_beg_(beg), buf_end_(end) {}
        Slice(const char* off, size_t n) : buf_beg_(off), buf_end_(off + n) {}
        Slice(const std::string& str) 
            : buf_beg_(str.data()), buf_end_(str.data() + str.size()) {}
        Slice(const char* str) : buf_beg_(str), buf_end_(str + strlen(str)) {}
    public:
        Slice GetWord();
        Slice RemoveLine();
        Slice Move(int size);
        Slice Advance(int beg_off, int end_off = 0) const;
        Slice& Trim();
        std::string ToString() const;
        int Compare(const Slice& buf) const;
        bool HasPrefix(const Slice& buf) const;
        int HasSuffix(const Slice& buf) const;
        operator std::string() const;
        void Split(char c, std::vector<Slice>& vec) const;

        const char* Data() const { return buf_beg_; }
        char* Data() { return const_cast<char*>(buf_beg_); }
        const char* Begin() const { return buf_beg_; }
        const char* End() const { return buf_end_; }

        char Front() { return *buf_beg_; }
        char Back() { return buf_end_[-1]; }
        size_t Size() const { return buf_end_ - buf_beg_; }
        void Resize(size_t size) { buf_beg_ = buf_beg_ + size; }
        inline bool Empty() const { return buf_beg_ == buf_end_; }
        void Clear() { buf_beg_ = buf_end_ = ""; }

        inline char operator[](size_t n) const
        { return buf_beg_[n]; }
    private:
        const char* buf_beg_;
        const char* buf_end_;
    };


    
}