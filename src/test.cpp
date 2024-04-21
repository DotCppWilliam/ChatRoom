#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <sstream>

#include "color_console_appender.h"
#include "init.h"
#include "log.h"
#include "logger.h"
#include "record.h"
#include "severity.h"
#include "txt_formatter.h"

using namespace std;


int main()
{
    logging::Init(logging::Severity::fatal);
    LOG_FMT_DEBUG_MSG("你好 %d", 1);
    LOG_FMT_WARNING_MSG("你好 %d", 1);
    
    return 0;
}

