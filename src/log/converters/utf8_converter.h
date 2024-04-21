#pragma once

#include <string>
#include <iconv.h>

namespace logging 
{
    namespace CodePage
    {
        const unsigned int kActive = 0;
        const unsigned int kUTF8 = 65001;
        const unsigned int kChar = kUTF8;
    }


    class UTF8Converter
    {
    public:
        static std::string Header(const std::string& str)
        {
            const char kBOM[] = "\xEF\xBB\xBF";
            return std::string(kBOM) + str;
        }

        static std::string Convert(const std::string& str)
        {
            return str;
        }
    };

}
