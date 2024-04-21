#pragma once

#include "color_console_appender.h"
#include "logger.h"
#include "severity.h"
#include "txt_formatter.h"

#include <memory>

namespace logging 
{
    inline Logger& Init(Severity max_severity = none, 
        std::shared_ptr<IAppender> appender = std::make_shared<logging::ColorConsoleAppender<logging::TxtFormatter>>())
    {
        Logger* logger = Logger::GetInstance();
        logger->set_max_severity(max_severity);
        return appender ? logger->AddAppender(appender) : *logger;
    }
}