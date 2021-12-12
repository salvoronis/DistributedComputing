#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "ipc.h"
#include "proc_info.h"
#include "io.h"
#include "log.h"
#include "pa1.h"

local_id currentProcId = -1;

int children_slave_work(struct SystemInfo * systemInfo) {
    log_pa(Event, log_started_fmt, 3, currentProcId, getpid(), getppid());
    char start_str[256];
    sprintf(start_str, log_started_fmt, currentProcId, getpid(), getppid());
    const Message msg = init_message(start_str, STARTED);
    if (send_multicast(systemInfo, &msg) != 0) {
        return -1;
    }
    Message messages_tmp;
    for (local_id i = 1; i < systemInfo->proc_count; i++) {
        if (currentProcId == i) {continue;}
        if (receive(systemInfo, i, &messages_tmp) != 0) {
            return -1;
        }
        if (messages_tmp.s_header.s_magic != MESSAGE_MAGIC &&
        messages_tmp.s_header.s_type != STARTED) {
            return -1;
        }
    }
    log_pa(Event, log_received_all_started_fmt, 1, currentProcId);
    //some work here

    //send done message
    log_pa(Event, log_done_fmt, 1, currentProcId);
    char end_str[256];
    sprintf(end_str, log_done_fmt, currentProcId);
    const Message done_msg = init_message(end_str, DONE);
    if (send_multicast(systemInfo, &done_msg) != 0) {
        return -1;
    }
    Message messages_done_tmp;
    for (local_id i = 1; i < systemInfo->proc_count; i++) {
        if (currentProcId == i) {continue;}
        if (receive(systemInfo, i, &messages_done_tmp) != 0) {
            return -1;
        }
        if (messages_done_tmp.s_header.s_magic != MESSAGE_MAGIC &&
            messages_done_tmp.s_header.s_type != DONE) {
            return -1;
        }
    }
    log_pa(Event, log_received_all_done_fmt, 1, currentProcId);
    return 0;
}

int close_pipes(struct SystemInfo systemInfo){
    for (local_id i = 0; i < systemInfo.proc_count; i++) {
        for (local_id j = 0; j < systemInfo.proc_count; j++) {
            if (i == j) {continue;}
            close(systemInfo.proccessesInfo[i].pipes[j][0]);
            close(systemInfo.proccessesInfo[i].pipes[j][1]);
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
        if (messages_started_tmp.s_header.s_magic != MESSAGE_MAGIC &&
        messages_started_tmp.s_header.s_type != STARTED) {
            return -1;
        }
    }
    log_pa(Event, log_received_all_started_fmt, 1, currentProcId);

    log_pa(Event, log_done_fmt, 1, currentProcId);
    Message messages_done_tmp;
    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        if (receive(&systemInfo, i, &messages_done_tmp) != 0) {
            return -1;
        }
        if (messages_done_tmp.s_header.s_magic != MESSAGE_MAGIC &&
                messages_done_tmp.s_header.s_type != DONE) {
            return -1;
        }
    }
    log_pa(Event, log_received_all_done_fmt, 1, currentProcId);
    return 0;
}

int close_unreachable_pipes(struct SystemInfo cur_sys_info){
    for (local_id i = 0; i < cur_sys_info.proc_count; i++) {
        for (local_id j = 0; j < cur_sys_info.proc_count; j++) {
            if (i == currentProcId) {
                close(cur_sys_info.proccessesInfo[i].pipes[j][0]);
                continue;
            } else if (j == currentProcId) {
                close(cur_sys_info.proccessesInfo[i].pipes[j][1]);
                continue;
            }
            close(cur_sys_info.proccessesInfo[i].pipes[j][1]);
            close(cur_sys_info.proccessesInfo[i].pipes[j][0]);
        }
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
            log_pa(Pipe, "i%d/o%d\n", 2,
                   systemInfo.proccessesInfo[i].pipes[j][1],
                   systemInfo.proccessesInfo[i].pipes[j][0]);
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
                close_unreachable_pipes(systemInfo);
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
    currentProcId = 0;
    close_unreachable_pipes(systemInfo);
    if (parent_work(systemInfo) != 0) {
        exit(EXIT_FAILURE);
    }
    for(local_id i = 0; i < systemInfo.proc_count; i++){
        wait(&systemInfo.proccessesInfo[i].proc_pid);
    }
    close_log();
    close_pipes(systemInfo);
    exit(EXIT_SUCCESS);
}
