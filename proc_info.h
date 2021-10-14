//
// Created by salvoroni on 10/14/21.
//

#ifndef PA1_PROC_INFO_H
#define PA1_PROC_INFO_H

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

struct ProcInfo {
    //pipes[0] - read [1] - write
    int     pipes[11][2];
    pid_t   proc_pid;
};

struct SystemInfo {
    int8_t  proc_count;
    struct ProcInfo proccessesInfo[11];
};

#endif //PA1_PROC_INFO_H
