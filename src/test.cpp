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
    LOG_FATAL_MSG("尝试将自身加入到 thread_group 中");
    LOG_FMT_ERROR_MSG("你好%d [%s]\n", 1, "aa");
    
    return 0;
}

