#include "conf.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <list>
#include <memory>
#include <string>

namespace net 
{
    struct LineScanner
    {
    public:
        LineScanner(char* line) : ptr_(line), err_(0) {}
        LineScanner& SkipSpaces()
        {
            while (!err_ && *ptr_ && isspace(*ptr_))
                ptr_++;
            return *this;
        }

        LineScanner& Skip(int i)
        { 
            ptr_ += i;
            return *this;
        }

        LineScanner& Match(char c)
        {
            SkipSpaces();
            err_ = *ptr_++ != c;
            return *this;
        }

        static std::string Rstrip(char* str, char* end)
        {
            while (end > str && isspace(end[-1]))
                end--;

            return { str, end };
        }

        int PeekChar()
        {
            SkipSpaces();
            return *ptr_;
        }

        std::string ConsumeTill(char c)
        {
            SkipSpaces();
            char* end = ptr_;
            while (!err_ && *end && *end != c)
                end++;

            if (*end != c)
            {
                err_ = 1;
                return "";
            }

            char* str = ptr_;
            ptr_ = end;
            return Rstrip(str, end);
        }

        std::string ConsumeTillEnd()
        {
            SkipSpaces();
            char* end = ptr_;
            int wasspace = 0;
            while (!err_ && *end && *end != ';' && *end != '#')
            {
                if (wasspace)
                    break;

                wasspace = isspace(*end);
                end++;
            }
            char* str = ptr_;
            ptr_ = end;

            return Rstrip(str, end);
        }
        
        char* ptr_;
        int err_;
    };






    static std::string MakeKey(const std::string& section, const std::string& name)
    {
        std::string key = section + "." + name;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        return key;
    }

    int Configure::Parse(const std::string& filename)
    {
        this->filename_ = filename;
        FILE* file = fopen(this->filename_.c_str(), "r");
        if (!file)
            return -1;
        std::unique_ptr<FILE, decltype(fclose)*> release(file, fclose);

        static const int kMaxLine = 16 * 1024;
        std::shared_ptr<char> line(new char[kMaxLine]);
        int line_no = 0;
        std::string section, key;
        int err = 0;
        while (!err && fgets(line.get(), kMaxLine, file) != nullptr)
        {
            line_no++;
            LineScanner scanner(line.get());
            int ch = scanner.PeekChar();
            if (ch == ';' || ch == '#' || ch == '\0')
                continue;
            else if (ch == '[')
            {
                section = scanner.Skip(1).ConsumeTill(']');
                err = scanner.Match(']').err_;
                key = "";
            }
            else if (isspace(line.get()[0]))
            {
                if (!key.empty())
                    values_[MakeKey(section, key)].push_back(scanner.ConsumeTill('\0'));
                else
                    err = 1;
            }
            else 
            {
                LineScanner ls = scanner;
                key = ls.ConsumeTill('=');
                if (ls.PeekChar() == '=')
                    ls.Skip(1);
                else
                {
                    scanner = ls;
                    key = scanner.ConsumeTill(':');
                    err = scanner.Match(':').err_;
                }
                std::string value = scanner.ConsumeTillEnd();
                values_[MakeKey(section, key)].push_back(value);
            }
        }
        return err ? line_no : 0;
    }

    std::string Configure::Get(const std::string& section, const std::string& name, 
        const std::string& def_val)
    {
        std::string key = MakeKey(section, name);
        auto it = values_.find(key);
        return it == values_.end() ? def_val : it->second.back();
    }

    long Configure::GetInteger(const std::string& section, const std::string& name, long def_val)
    {
        std::string val_str = Get(section, name, "");
        const char* value = val_str.c_str();
        char* end;

        long n = strtol(value, &end, 0);
        return end > value ? n : def_val;
    }

    double Configure::GetReal(const std::string& section, const std::string& name, double def_val)
    {
        std::string val_str = Get(section, name, "");
        const char* value = val_str.c_str();
        char* end;
        double n = strtod(value, &end);
        return end > value ? n : def_val;
    }

    bool Configure::GetBoolean(const std::string& section, const std::string& name, bool def_val)
    {
        std::string val_str = Get(section, name, "");
        std::transform(val_str.begin(), val_str.end(), val_str.begin(), ::tolower);

        if (val_str == "true" || val_str == "yes" || val_str == "on" || val_str == "1")
            return true;
        else if (val_str == "false" || val_str == "no" || val_str == "off" || val_str == "0")
            return false;
        else
            return def_val;
    }

    std::list<std::string> Configure::GetStrings(const std::string& section, 
        const std::string& name)
    {
        std::string key = MakeKey(section, name);
        auto it = values_.find(key);
        return it == values_.end() ? std::list<std::string>() : it->second;
    }
}