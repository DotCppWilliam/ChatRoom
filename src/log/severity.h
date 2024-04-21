#pragma once

#include <cctype>

namespace logging
{

// 控制台颜色显示
    // fatal:   粗体 + 红底白字
    // error:   粗体 + 红字
    // warning: 黄字
    // info:    绿字
    // debug:   蓝字
    // verbose: 青色
    #define ANSI_BOLD       "\033[1m"
    #define ANSI_RED_BG     "\033[41m"
    #define ANSI_RED        "\033[31m"
    #define ANSI_WHITE      "\033[97m"
    #define ANSI_YELLOW     "\033[33m"
    #define ANSI_BLUE       "\033[34m"
    #define ANSI_GREEN      "\033[32m"
    #define ANSI_CYAN       "\033[36m"
    #define ANSI_RESET      "\033[0m"

    enum Severity   // 日志的严重程度
    {
        none = -1,
        fatal = 5,
        error = 4,
        warning = 3,
        info = 2,
        debug = 1,
        verbose = 0
    };

    /**
     * @brief 将日志级别转换成字符串
     * 
     * @param severity 
     * @return const char* 
     */
    inline const char* SeverityToString(Severity severity)
    {
        switch (severity) 
        {
            case fatal:     return "FATAL";
            case error:     return "error";
            case warning:    return "WARN";
            case info:      return "INFO";
            case debug:     return "DEBUG";
            case verbose:   return "VERBOSE";
            default:        return "NONE";
        }
        return "NONE";
    }

    /**
     * @brief 将字符串转换成日志级别枚举值
     * 
     * @param str 
     * @return Severity 
     */
    inline Severity SeverityFromString(const char* str)
    {
        switch (std::toupper(str[0])) 
        {
            case 'F':   return fatal;
            case 'e':   return error;
            case 'w':   return warning;
            case 'i':   return info;
            case 'd':   return debug;
            case 'v':   return verbose;
            default:    return none;
        }
        return none;
    }
}