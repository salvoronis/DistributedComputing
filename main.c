#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "ipc.h"
#include "proc_info.h"
#include "io.h"

local_id currentProcId = -1;

int children_slave_work(struct SystemInfo * systemInfo) {
    printf("%d started\n", currentProcId);
    //send started message to other processes
    const Message msg = init_message("anime", STARTED);
    if (send_multicast(systemInfo, &msg) != 0) {
        return -1;
    }
    printf("%d sended multicast message\n", currentProcId);
    Message messages_tmp;
    for (local_id i = 1; i < systemInfo->proc_count; i++) {
        if (currentProcId == i) {continue;}
        if (receive(systemInfo, i, &messages_tmp) != 0) {
            return -1;
        }
        if (messages_tmp.s_header.s_type == STARTED) {
            printf("%d: received %d magic\n", currentProcId, messages_tmp.s_header.s_magic);
        }
    }
    printf("process %d received all starts\n", currentProcId);
    //some work here

    //send done message
    const Message done_msg = init_message("anime", DONE);
    if (send_multicast(systemInfo, &done_msg) != 0) {
        return -1;
    }
    Message messages_done_tmp;
    for (local_id i = 1; i < systemInfo->proc_count; i++) {
        if (currentProcId == i) {continue;}
        if (receive(systemInfo, i, &messages_done_tmp) != 0) {
            return -1;
        }
        if (messages_done_tmp.s_header.s_type == DONE) {
            printf("%d: received done\n", currentProcId);
        }
    }
    return 0;
}

int parent_work(struct SystemInfo systemInfo) {
    Message messages_started_tmp;
    currentProcId = 0;
    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        if (receive(&systemInfo, i, &messages_started_tmp) != 0) {
            return -1;
        }
        if (messages_started_tmp.s_header.s_type == STARTED) {
            printf("%d: received start\n", currentProcId);
        }
    }

    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        if (receive(&systemInfo, i, &messages_started_tmp) != 0) {
            return -1;
        }
        if (messages_started_tmp.s_header.s_type == DONE) {
            printf("%d: received done\n", currentProcId);
        }
    }
    return 0;
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
                if (children_slave_work(&systemInfo) != 0 ) {
                    exit(EXIT_FAILURE);
                }
                exit(EXIT_SUCCESS);
            }
            default: {
                continue;
            }
        }
    }
    if (parent_work(systemInfo) != 0) {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}