#include "file.h"

#include <cstring>
#include <sys/types.h>
#include <dirent.h>

namespace util 
{
    inline const char* FindExtensionDot(const char* file_name)
    {
        return ::strrchr(file_name, '.');
    }

    void SplitFileName(const char* file_name, 
        std::string& filename_no_exist, 
        std::string& file_exist)
    {
        const char* dot = FindExtensionDot(file_name);

        if (dot)
        {
            filename_no_exist.assign(file_name, dot);
            file_exist.assign(dot + 1);
        }
        else 
        {
            filename_no_exist.assign(file_name);
            file_exist.clear();
        }
    }


    std::string ProcessFuncName(const char* func)
    {
        const char* func_begin = func;
        const char* func_end = ::strchr(func_begin, '(');
        int found_template = 0;

        if (!func_end)
            return std::string(func);

        for (const char* i = func_end - 1; i >= func_begin; --i)
        {
            if (*i == '>')
                found_template++;
            else if (*i == '<')
                found_template--;
            else if (*i == ' ' && found_template == 0)
            {
                func_begin = i + 1;
                break;
            }
        }

        return std::string(func_begin, func_end);
    }






    using namespace std;
    Status File::GetContent(const std::string &filename, std::string &cont) 
    {
        int fd = open(filename.c_str(), O_RDONLY);
        if (fd < 0) {
            return Status::IoError("open", filename);
        }
        util::ExitCaller ec1([=] { close(fd); });
        char buf[4096];
        for (;;) {
            int r = read(fd, buf, sizeof buf);
            if (r < 0) {
                return Status::IoError("read", filename);
            } else if (r == 0) {
                break;
            }
            cont.append(buf, r);
        }
        return Status();
    }

    Status File::WriteContent(const std::string &filename, const std::string &cont) 
    {
        int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            return Status::IoError("open", filename);
        }
        util::ExitCaller ec1([=] { close(fd); });
        int r = write(fd, cont.data(), cont.size());
        if (r < 0) {
            return Status::IoError("write", filename);
        }
        return Status();
    }

    Status File::RenameSave(const string &name, const string &tmpName, const string &cont) 
    {
        Status s = WriteContent(tmpName, cont);
        if (s.Ok()) {
            unlink(name.c_str());
            s = RenameFile(tmpName, name);
        }
        return s;
    }

    Status File::GetChildren(const std::string &dir, std::vector<std::string> *result) 
    {
        result->clear();
        DIR *d = opendir(dir.c_str());
        if (d == NULL) {
            return Status::IoError("opendir", dir);
        }
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            result->push_back(entry->d_name);
        }
        closedir(d);
        return Status();
    }

    Status File::DeleteFile(const string &fname) 
    {
        if (unlink(fname.c_str()) != 0) {
            return Status::IoError("unlink", fname);
        }
        return Status();
    }

    Status File::CreateDir(const std::string &name) {
        if (mkdir(name.c_str(), 0755) != 0) {
            return Status::IoError("mkdir", name);
        }
        return Status();
    }

    Status File::DeleteDir(const std::string &name) {
        if (rmdir(name.c_str()) != 0) {
            return Status::IoError("rmdir", name);
        }
        return Status();
    }

    Status File::GetFileSize(const std::string &fname, uint64_t *size) 
    {
        struct stat sbuf;
        if (stat(fname.c_str(), &sbuf) != 0) {
            *size = 0;
            return Status::IoError("stat", fname);
        } else {
            *size = sbuf.st_size;
        }
        return Status();
    }

    Status File::RenameFile(const std::string &src, const std::string &target) 
    {
        if (rename(src.c_str(), target.c_str()) != 0) {
            return Status::IoError("rename", src + " " + target);
        }
        return Status();
    }

    bool File::FileExists(const std::string &fname) {
        return access(fname.c_str(), F_OK) == 0;
    }
}