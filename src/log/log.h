#pragma once

#include "logger.h"
#include "record.h"
#include "severity.h"
#include "init.h"
#include "color_console_appender.h"
#include "console_appender.h"
#include "dynamic_appender.h"
#include "rolling_file_appender.h"

#include "csv_formatter.h"
#include "func_message_formatter.h"
#include "message_only_formatter.h"
#include "txt_formatter.h"
#include "../appenders/iappender.h"



#define LOG_GET_THIS()  reinterpret_cast<void*>(0)
#define LOG_GET_LINE()  __LINE__
#define LOG_GET_FUNC()  __FUNCTION__
#define LOG_GET_FILE()  __FILE__ 

// 指定日志级别,但是没有指定日志消息
#define LOG(severity) \
    (*logging::Logger::GetInstance()) += logging::Record(severity, LOG_GET_FUNC(), \
        LOG_GET_LINE(), LOG_GET_FILE(), LOG_GET_THIS()).Ref()

// 指定日志级别,输出指定消息
#define LOG_SEVERITY_MSG(severity, msg) \
    (*logging::Logger::GetInstance()) += logging::Record(severity, msg).Ref()

// 输出指定消息
#define LOG_MSG(msg) \
    (*logging::Logger::GetInstance()) += logging::Record(msg).Ref()

// 格式化日志输出指定消息
#define LOG_FMT_MSG(format, ...) \
    (*logging::Logger::GetInstance()) += logging::Record(format, __VA_ARGS__).Ref()

// 指定日志级别的格式化日志输出消息
#define LOG_FMT_SEVERITY_MSG(severity, format, ...) \
    (*logging::Logger::GetInstance()) += logging::Record(severity, format, ##__VA_ARGS__).Ref()



// 输出具体级别的日志
#define LOG_VERBOSE LOG(logging::verbose)
#define LOG_DEBUG   LOG(logging::debug)
#define LOG_INFO    LOG(logging::info)
#define LOG_WARNING LOG(logging::warning)
#define LOG_ERROR   LOG(logging::error)
#define LOG_FATAL   LOG(logging::fatal)
#define LOG_NONE    LOG(logging::none)

#define LOG_VERBOSE_MSG(msg)    LOG_SEVERITY_MSG(logging::verbose, msg)
#define LOG_DEBUG_MSG(msg)      LOG_SEVERITY_MSG(logging::debug, msg)
#define LOG_INFO_MSG(msg)       LOG_SEVERITY_MSG(logging::info, msg)
#define LOG_WARNING_MSG(msg)    LOG_SEVERITY_MSG(logging::warning, msg)
#define LOG_ERROR_MSG(msg)      LOG_SEVERITY_MSG(logging::error, msg)
#define LOG_FATAL_MSG(msg)      LOG_SEVERITY_MSG(logging::fatal, msg)
#define LOG_NONE_MSG(msg)       LOG_SEVERITY_MSG(logging::none, msg)


#define LOG_FMT_VERBOSE_MSG(format, ...)   LOG_FMT_SEVERITY_MSG(logging::verbose, format, __VA_ARGS__)
#define LOG_FMT_DEBUG_MSG(format, ...)     LOG_FMT_SEVERITY_MSG(logging::debug, format, __VA_ARGS__)
#define LOG_FMT_INFO_MSG(format, ...)      LOG_FMT_SEVERITY_MSG(logging::info, format, __VA_ARGS__)
#define LOG_FMT_WARNING_MSG(format, ...)   LOG_FMT_SEVERITY_MSG(logging::warning, format, __VA_ARGS__)
#define LOG_FMT_ERROR_MSG(format, ...)     LOG_FMT_SEVERITY_MSG(logging::error, format, __VA_ARGS__)
#define LOG_FMT_FATAL_MSG(format, ...)     LOG_FMT_SEVERITY_MSG(logging::fatal, format, __VA_ARGS__)
#define LOG_FMT_NONE_MSG(format, ...)      LOG_FMT_SEVERITY_MSG(logging::none, format, __VA_ARGS__)


// 条件日志宏
#define LOG_IF(severity) \
    if (!logging::Logger::GetInstance()->CheckSeverity(severity)) {; } else

#define LOG_COND_IF(severity, condition) \
    if (!(condition)) {;} else PLOG(severity)

#define LOG_VERBOSE_IF(condition)   LOG_COND_IF(logging::verbose, condition)
#define LOG_DEBUG_IF(condition)     LOG_COND_IF(logging::debug,  condition)
#define LOG_INFO_IF(condition)      LOG_COND_IF(logging::info, condition)
#define LOG_WARNING_IF(condition)   LOG_COND_IF(logging::warning, condition)
#define LOG_ERROR_IF(condition)     LOG_COND_IF(logging::error, condition)
#define LOG_FATAL_IF(condition)     LOG_COND_IF(logging::fatal, condition)
#define LOG_NONE_IF(condition)      LOG_COND_IF(logging::none, condition)

#define LOGV_IF(condition)      LOG_VERBOSE_IF(condition)
#define LOGD_IF(condition)      LOG_DEBUG_IF(condition)
#define LOGI_IF(condition)      LOG_INFO_IF(condition)
#define LOGW_IF(condition)      LOG_WARNING_IF(condition)
#define LOGE_IF(condition)      LOG_ERROR_IF(condition)
#define LOGF_IF(condition)      LOG_FATAL_IF(condition)
#define LOGN_IF(condition)      LOG_NONE_IF(condition)

