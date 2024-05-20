#include "conn.h"
#include "log.h"

using namespace std;


int main()
{
    logging::Init(logging::Severity::fatal);
    LOG_FATAL_MSG("尝试将自身加入到 thread_group 中");
    LOG_FMT_ERROR_MSG("你好%d [%s]\n", 1, "aa");
    
    net::TcpConnPtr tcp;

    return 0;
}

