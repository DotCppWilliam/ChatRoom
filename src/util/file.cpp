#include "file.h"

#include <cstring>

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

}