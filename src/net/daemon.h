#pragma once

#include <functional>

namespace net 
{
    struct Daemon 
    {
        static int DaemonStart(const char *pidfile);
        // exit in parent
        static int DaemonRestart(const char *pidfile);
        static int DaemonStop(const char *pidfile);
        static int GetPidFromFile(const char *pidfile);

        // cmd: start stop restart
        // exit(1) when error
        // exit(0) when start or restart in parent
        // return when start or restart in child
        static void DaemonProcess(const char *cmd, const char *pidfile);
        // fork; wait for parent to exit; exec argv
        // you may use it to implement restart in program
        static void ChangeTo(const char *argv[]);
    };

    struct Signal 
    {
        static void signal(int sig, const std::function<void()> &handler);
    };
}