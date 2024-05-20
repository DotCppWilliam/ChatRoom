#pragma once

#include "noncopyable.h"
#include "status.h"

#include <cstddef>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

namespace util
{
    void SplitFileName(const char* file_name, 
        std::string& filename_no_exist, 
        std::string& file_exist);

    inline const char* FindExtensionDot(const char* file_name);

    class File : public util::NonCopyable
    {
    public:
        File() : file_(-1) {}
        ~File() { Close(); }

        size_t Open(const std::string& filename)
        {
            file_ = ::open(filename.c_str(), 
                O_CREAT | O_APPEND | O_WRONLY, 
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

            return Seek(0, SEEK_END);
        }

        size_t Write(const void* buf, size_t count)
        {
            return file_ != -1 ? static_cast<size_t>(::write(file_, buf, count)) 
                : static_cast<size_t>(-1);
        }

        template <class CharType>
        size_t Write(const std::basic_string<CharType>& str)
        {
            return Write(str.data(), str.size() * sizeof(CharType));
        }

        size_t Seek(size_t offset, int whence)
        {
            return file_ != -1 
                ? static_cast<size_t>(::lseek(file_, static_cast<off_t>(offset), whence)) 
                : static_cast<size_t>(-1);
        }

        void Close()
        {
            if (file_ != -1)
            {
                ::close(file_);
                file_ = -1;
            }
        }

        static int Unlink(const std::string filename)
        {
            return ::unlink(filename.c_str());
        }

        static int Rename(const std::string& old_filename, const std::string& new_filename)
        {
            return ::rename(old_filename.c_str(), new_filename.c_str());
        }

        static Status GetContent(const std::string &filename, std::string &cont);
        static Status WriteContent(const std::string &filename, const std::string &cont);
        static Status RenameSave(const std::string &name, const std::string &tmpName, const std::string &cont);
        static Status GetChildren(const std::string &dir, std::vector<std::string> *result);
        static Status DeleteFile(const std::string &fname);
        static Status CreateDir(const std::string &name);
        static Status DeleteDir(const std::string &name);
        static Status GetFileSize(const std::string &fname, uint64_t *size);
        static Status RenameFile(const std::string &src, const std::string &target);
        static bool FileExists(const std::string &fname);
    private:
        int file_;
    };



    std::string ProcessFuncName(const char* func);
}
