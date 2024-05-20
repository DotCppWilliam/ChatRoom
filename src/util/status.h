#pragma once

#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <string>

#include "util.h"

namespace util 
{
    inline const char *errstr() 
    {
        return strerror(errno);
    }

    struct Status 
    {
        // Create a success status.
        Status() : state_(NULL) {}
        Status(int code, const char *msg);
        Status(int code, const std::string &msg) : Status(code, msg.c_str()) {}
        ~Status() { delete[] state_; }

        // Copy the specified status.
        Status(const Status &s) { state_ = CopyState(s.state_); }
        void operator=(const Status &s) 
        {
            delete[] state_;
            state_ = CopyState(s.state_);
        }
        Status(Status &&s) 
        {
            state_ = s.state_;
            s.state_ = NULL;
        }
        void operator=(Status &&s) 
        {
            delete[] state_;
            state_ = s.state_;
            s.state_ = NULL;
        }

        static Status FromSystem() { return Status(errno, strerror(errno)); }
        static Status FromSystem(int err) { return Status(err, strerror(err)); }
        static Status FromFormat(int code, const char *fmt, ...);
        static Status IoError(const std::string &op, const std::string &name) { return Status::FromFormat(errno, "%s %s %s", op.c_str(), name.c_str(), errstr()); }

        int Code() { return state_ ? *(int32_t *) (state_ + 4) : 0; }
        const char *Msg() { return state_ ? state_ + 8 : ""; }
        bool Ok() { return Code() == 0; }
        std::string ToString() { return util::Format("%d %s", Code(), Msg()); }

    private:
        //    state_[0..3] == length of message
        //    state_[4..7]    == code
        //    state_[8..]  == message
        const char *state_;
        const char *CopyState(const char *state);
    };

    inline const char *Status::CopyState(const char *state) 
    {
        if (state == NULL) 
        {
            return state;
        }
        uint32_t size = *(uint32_t *) state;
        char *res = new char[size];
        memcpy(res, state, size);
        return res;
    }

    inline Status::Status(int code, const char *msg) 
    {
        uint32_t sz = strlen(msg) + 8;
        char *p = new char[sz];
        state_ = p;
        *(uint32_t *) p = sz;
        *(int32_t *) (p + 4) = code;
        memcpy(p + 8, msg, sz - 8);
    }

    inline Status Status::FromFormat(int code, const char *fmt, ...) 
    {
        va_list ap;
        va_start(ap, fmt);
        uint32_t size = 8 + vsnprintf(NULL, 0, fmt, ap) + 1;
        va_end(ap);
        Status r;
        r.state_ = new char[size];
        *(uint32_t *) r.state_ = size;
        *(int32_t *) (r.state_ + 4) = code;
        va_start(ap, fmt);
        vsnprintf((char *) r.state_ + 8, size - 8, fmt, ap);
        va_end(ap);
        return r;
    }
}