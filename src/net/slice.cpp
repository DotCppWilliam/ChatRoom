#include "slice.h"
#include <cctype>
#include <vector>

namespace net 
{
    /**
     * @brief 返回一个Word不包含空格
     * 
     * @return Slice 
     */
    Slice Slice::GetWord()
    {
        const char* beg = buf_beg_;

        // 去掉字符串开头的空格
        while (beg < buf_end_ && std::isspace(*beg))
            beg++;
        
        // 一直到有空格的地方
        const char* end = beg;
        while (end < buf_end_ && !std::isspace(*beg))
            end++;

        buf_beg_ = end;

        // 返回一个Word,不包含空格
        return Slice(beg, end - beg);
    }

    /**
     * @brief 去掉换行符
     * 
     * @return Slice 
     */
    Slice Slice::RemoveLine()
    {
        const char* beg = buf_beg_;
        while (buf_beg_ < buf_end_ && *buf_beg_ != '\n' && *buf_beg_ != '\r')
            buf_beg_++;

        return Slice(beg, buf_beg_ - beg);
    }

    Slice Slice::Move(int size)
    {
        Slice buf(buf_beg_, size);
        buf_end_ += size;
        return buf;
    }

    /**
     * @brief 将缓冲区的首部或尾部向后移动
     * 
     * @param beg_off 
     * @param end_off 
     * @return Slice 
     */
    Slice Slice::Advance(int beg_off, int end_off) const 
    {
        Slice buf(*this);
        buf.buf_beg_ += beg_off;
        buf.buf_end_ += end_off;

        return buf;
    }

    /**
     * @brief 去除两边空格
     * 
     * @return Slice& 
     */
    inline Slice& Slice::Trim()
    {
        while (buf_beg_ < buf_end_ && isspace(*buf_beg_))
            buf_beg_++;

        while (buf_beg_ < buf_end_ && isspace(buf_end_[-1]))
            buf_end_--;

        return *this;
    }

    

    std::string Slice::ToString() const 
    { return std::string(buf_beg_, buf_end_); }




    /**
     * @brief 比较两个缓冲区是否相等
     * 
     * @param buf 
     * @return int -1: 不相等, 1相等
     */
    int Slice::Compare(const Slice& buf) const 
    {
        size_t size = Size();
        size_t rhs_size = buf.Size();
        const int min_len = (size < rhs_size) ? size : rhs_size;
        int ret = memcmp(buf_beg_, buf.buf_beg_, min_len);
        if (ret == 0)
        {
            if (size < rhs_size)
                ret = -1;
            else if (size > rhs_size)
                ret = +1;
        }
        return ret;
    }


    /**
     * @brief 当前缓冲区是否以指定缓冲区的内容开头
     * 
     * @param buf 
     * @return true 
     * @return false 
     */
    bool Slice::HasPrefix(const Slice& buf) const 
    {
        return (Size() >= buf.Size() 
            && memcmp(buf_beg_, buf.buf_beg_, buf.Size()) == 0);
    }

    /**
     * @brief 检查当前缓冲区是否以指定缓冲区的内容结尾
     * 
     * @param buf 
     * @return int 
     */
    int Slice::HasSuffix(const Slice& buf) const 
    {
        return (Size() >= buf.Size() 
            && memcmp(buf_end_ - buf.Size(), buf.buf_beg_, buf.Size()) == 0);
    }

    Slice::operator std::string() const 
    {
        return std::string(buf_beg_, buf_end_);
    }


    /**
     * @brief 根据ch字符分割整个字符串
     * 
     * @param ch 
     * @param vec 
     */
    void Slice::Split(char ch, std::vector<Slice>& vec) const 
    {
        const char* beg = buf_beg_;
        for (const char* ptr = buf_beg_; ptr < buf_end_; ++ptr)
        {
            if (*ptr == ch)
            {
                vec.push_back(Slice(beg, ptr));
                beg = ptr + 1;
            }
        }

        if (buf_end_ != buf_beg_)
            vec.push_back(Slice(beg, buf_end_));
    }
}