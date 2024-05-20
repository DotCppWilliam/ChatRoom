#pragma once

#include <string>
#include <list>
#include <unordered_map>

namespace net 
{
    struct Configure
    {
        int Parse(const std::string& filename);
        std::string Get(const std::string& section, const std::string& name, const std::string& def_val);
        long GetInteger(const std::string& section, const std::string& name, long def_val);
        double GetReal(const std::string& section, const std::string& name, double def_val);
        bool GetBoolean(const std::string& section, const std::string& name, bool def_val);

        std::list<std::string> GetStrings(const std::string& section, 
            const std::string& name);
        std::unordered_map<std::string, std::list<std::string>> values_;
        std::string filename_;
    };
    
}