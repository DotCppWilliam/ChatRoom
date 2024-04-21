#pragma once

#include "iappender.h"
#include "utf8_converter.h"
#include "auto_lock.h"
#include "file.h"
#include <cstddef>
#include <sstream>

namespace logging 
{
    template <typename Formatter, typename Converter = UTF8Converter>
    class RollingFileAppender : public IAppender
    {
    public:
        RollingFileAppender(const char* file_name, size_t max_file_size = 0,
            int max_files = 0) 
            : file_size_(0),
              max_file_size_(0),
              max_files_(0),
              first_write_(true)
        {
            set_filename(file_name);
            set_max_file_size(max_file_size);
            this->appender_type_ = rolling_file_appender;
        }
        
        void Write(const Record& record) override
        {
            util::AutoLock lock(this->lock_);

            if (first_write_)
            {
                OpenLogFile();
                first_write_ = false;
            }
            else if (max_files_ > 0 
                && file_size_ > max_file_size_
                && static_cast<size_t>(-1) != file_size_)
            {
                RollLogFiles();
            }

            size_t bytes_written = file_.Write(Converter::Convert(Formatter::Format(record)));

            if (static_cast<size_t>(-1) != bytes_written)
                file_size_ += bytes_written;
        }

        void set_filename(const char* file_name)
        {
            util::AutoLock lock(this->lock_);
            util::SplitFileName(file_name, file_name_no_ext, file_ext_);

            file_.Close();
            first_write_ = true;
        }

        void set_max_file_size(int max_files)
        {
            max_file_size_ = max_files;
        }

        void set_max_files(int max_files)
        {
            max_files_ = max_files;
        }

    private:
        std::string BuildFileName(int file_number = 0)
        {
            std::ostringstream oss;
            oss << file_name_no_ext;

            if (file_number > 0)
                oss << '.' << file_number;
            
            if (!file_ext_.empty())
                oss << '.' << file_ext_;

            return oss.str();
        }

        void RollLogFiles()
        {
            file_.Close();

            std::string last_filename = BuildFileName(max_files_ - 1);
            util::File::Unlink(last_filename);

            for (int file_number = max_files_ - 2; file_number >= 0; --file_number)
            {
                std::string curr_filename = BuildFileName(file_number);
                std::string next_filename = BuildFileName(file_number + 1);

                util::File::Rename(curr_filename, next_filename);
            }
        }

        void OpenLogFile()
        {
            file_size_ = file_.Open(BuildFileName());

            if (file_size_ == 0)
            {
                size_t bytes_written = file_.Write(Converter::Header(Formatter::Header()));
                if (static_cast<size_t>(-1) != bytes_written)
                    file_size_ += bytes_written;
            }
        }

    private:
        util::Lock      lock_;
        util::File      file_;
        size_t          file_size_;
        size_t          max_file_size_;
        int             max_files_;
        std::string     file_ext_;          // 包含扩展名的文件名
        std::string     file_name_no_ext;   // 不包含扩展名的文件名
        bool            first_write_;
    };
}