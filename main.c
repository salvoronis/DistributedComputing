#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "ipc.h"
#include "proc_info.h"
#include "io.h"
#include "log.h"
#include "pa1.h"

local_id currentProcId = -1;

int children_slave_work(struct SystemInfo * systemInfo) {
    const Message msg = init_message("", STARTED);
    if (send_multicast(systemInfo, &msg) != 0) {
        return -1;
    }
    log_pa(Event, msg.s_payload, 0);
    Message messages_tmp;
    for (local_id i = 1; i < systemInfo->proc_count; i++) {
        if (currentProcId == i) {continue;}
        if (receive(systemInfo, i, &messages_tmp) != 0) {
            return -1;
        }
        if (messages_tmp.s_header.s_type == STARTED) {
            log_pa(Debug, "Process with local id: %d received message from process with local id: %d. Message: %s\n",
                   3, currentProcId, i, msg.s_payload);
        }
    }
    //some work here

    //send done message
    const Message done_msg = init_message("", DONE);
    if (send_multicast(systemInfo, &done_msg) != 0) {
        return -1;
    }
    log_pa(Event, msg.s_payload, 0);
    Message messages_done_tmp;
    for (local_id i = 1; i < systemInfo->proc_count; i++) {
        if (currentProcId == i) {continue;}
        if (receive(systemInfo, i, &messages_done_tmp) != 0) {
            return -1;
        }
        if (messages_done_tmp.s_header.s_type == DONE) {
            log_pa(Debug, "Process with local id: %d received message from process with local id: %d. Message: %s\n",
                   3, currentProcId, i, msg.s_payload);
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
        log_pa(Debug, "Process with local id: %d received message from process with local id: %d. Message: %s\n",
               3, currentProcId, i, messages_started_tmp.s_payload);
    }

    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        if (receive(&systemInfo, i, &messages_started_tmp) != 0) {
            return -1;
        }
        log_pa(Debug, "Process with local id: %d received message from process with local id: %d. Message: %s\n",
               3, currentProcId, i, messages_started_tmp.s_payload);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    init_log();
    struct SystemInfo systemInfo;
    // 0 process is always main
    systemInfo.proccessesInfo[0].proc_pid = getpid();
    systemInfo.proc_count = strtol(argv[2], NULL, 10) + 1;
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
    close_log();
    exit(EXIT_SUCCESS);
}
