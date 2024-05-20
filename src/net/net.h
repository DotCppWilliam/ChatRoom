#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <string>

namespace net
{
    static const int kLittleEndian = LITTLE_ENDIAN;

    inline uint16_t htobe(uint16_t val)
    {
        if (!kLittleEndian)
            return val;
        unsigned char* ptr_val = (unsigned char*)&val;
        return uint16_t(ptr_val[0]) << 8 | uint16_t(ptr_val[1]);
    }

    inline uint32_t htobe(uint32_t val)
    {
        if (!kLittleEndian)
            return val;

        unsigned char* ptr_val = (unsigned char*)&val;
        return uint32_t(ptr_val[0]) << 24 | uint32_t(ptr_val[1]) << 16
            | uint32_t(ptr_val[2]) << 8 | uint32_t(ptr_val[3]);
    }

    inline uint64_t htobe(uint64_t val)
    {
        if (!kLittleEndian)
            return val;

        unsigned char* ptr_val = (unsigned char*)&val;
        return uint64_t(ptr_val[0]) << 56 | uint64_t(ptr_val[1]) << 48
            | uint64_t(ptr_val[2]) << 40 | uint64_t(ptr_val[3]) << 32
            | uint64_t(ptr_val[4]) << 24 | uint64_t(ptr_val[5]) << 16
            | uint64_t(ptr_val[6]) << 8 | uint64_t(ptr_val[7]);
    }

    inline int16_t htobe(int16_t val)
    {
        return (int16_t) htobe((uint16_t)val);
    }

    inline int32_t htobe(int32_t val)
    { return (int32_t) htobe((uint32_t)val); }

    inline int64_t htobe(int64_t val)
    { return (int64_t) htobe((uint64_t)val); }

    struct in_addr GetHostByname(const std::string& host);

    uint64_t Gettid();

    template <typename T>
    static T ntoh(T val)
    { return htobe(val); }


    template <typename T>
    static T hton(T val)
    { return htobe(val); }

    int SetNonBlock(int fd, bool value = true);
    int SetReuseAddr(int fd, bool value = true);
    int SetReusePort(int fd, bool value = true);
    int SetNoDelay(int fd, bool value = true);
}