#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"
#include "ipc.h"
#include "pa1.h"

struct ProcInfo {
    //pipes[0] - read [1] - write
    int     pipes[11][2];
    pid_t   proc_pid;
};

struct SystemInfo {
    int8_t  proc_count;
    struct ProcInfo proccessesInfo[11];
};

local_id currentProcId = -1;

int send(void * self, local_id dst, const Message * msg) {
    struct SystemInfo * sysInfo = (struct SystemInfo *) self;
    if (currentProcId == -1) {
        return -1;
    }
    ssize_t res = write(
            sysInfo->proccessesInfo[currentProcId].pipes[dst][1],
            msg,
            sizeof(MessageHeader) + msg->s_header.s_payload_len);
    if (res == -1) {
        return -1;
    }
    return 0;
}

int send_multicast(void * self, const Message * msg) {
    struct SystemInfo * sysInfo = (struct SystemInfo *) self;
    if (currentProcId == -1) {
        return -1;
    }
    for (local_id i = 0; i < sysInfo->proc_count; i ++) {
        if (i == currentProcId) {continue;}
        if (send(sysInfo, i, msg) != 0) {
            return -1;
        }
    }
    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    struct SystemInfo * sysInfo = (struct SystemInfo *) self;
    if (currentProcId == -1) {
        return -1;
    }
    ssize_t res = read(sysInfo->proccessesInfo[from].pipes[currentProcId][0], msg, sizeof(Message));
    printf("%s\n", msg->s_payload);
    if (res == -1) {
        return -1;
    }
    return 0;
}

Message init_message(char *data, MessageType type) {
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = strlen(data);
    msg.s_header.s_type = type;
    strncpy(msg.s_payload, data, strlen(data));
    return msg;
}

int main(int argc, char *argv[]) {
    struct SystemInfo systemInfo;
    // 0 process is always main
    systemInfo.proccessesInfo[0].proc_pid = getpid();
    systemInfo.proc_count = strtol(argv[1], NULL, 10) + 1;
    for (int i = 0; i < systemInfo.proc_count; i++) {
        for (int j = 0; j < systemInfo.proc_count; j++) {
            if (i == j) {continue;}
            if (pipe(systemInfo.proccessesInfo[i].pipes[j]) == -1) {
                perror("pipe error");
                exit(EXIT_FAILURE);
            }
        }
    }
    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        systemInfo.proccessesInfo[i].proc_pid = fork();
        switch (systemInfo.proccessesInfo[i].proc_pid) {
            case -1: {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            case 0: {
                //start proc
                currentProcId = i;
                if (i == 1) {
                    const Message msg = init_message("Hello world", STARTED);
                    //send(&systemInfo, 4, &msg);
                    send_multicast(&systemInfo, &msg);
                } else {
                    Message msg;
                    receive(&systemInfo, 1, &msg);
                }
                exit(EXIT_SUCCESS);
            }
            default: {
                continue;
            }
        }
    }
    return 0;
}