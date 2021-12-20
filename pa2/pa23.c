#include "banking.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "ipc.h"
#include "proc_info.h"
#include "io.h"
#include "log.h"
#include "pa2345.h"

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount){
    // student, please implement me
    struct SystemInfo * system_info = (struct SystemInfo*) parent_data;
    TransferOrder transfer;
    transfer.s_amount = amount;
    transfer.s_src = src;
    transfer.s_dst = dst;
    const Message transfer_msg = init_message(&transfer, TRANSFER, sizeof(TransferOrder));

    if (send(system_info, transfer.s_src, &transfer_msg) != 0) {
        perror("transfer error");
        exit(-1);
    }
    Message ack_msg;
    if (receive(system_info, dst, &ack_msg) != 0) {
        perror("transfer error");
        exit(-1);
    }
}

local_id currentProcId = -1;
balance_t local_balance = 0;

int transfer_work(void * self, Message * msg) {
    TransferOrder * transfer = (TransferOrder*) msg->s_payload;
    if (currentProcId == transfer->s_src) {
        local_balance -= transfer->s_amount;
        log_pa(Event, log_transfer_out_fmt, msg->s_header.s_local_time, transfer->s_src, transfer->s_amount, transfer->s_dst);
        if (send(self, transfer->s_dst, msg) < 0) {
            perror("send error");
            exit(EXIT_FAILURE);
        }
    } else if (currentProcId == transfer->s_dst) {
        local_balance += transfer->s_amount;
        log_pa(Event, log_transfer_in_fmt, msg->s_header.s_local_time, transfer->s_dst, transfer->s_amount, transfer->s_src);
        const Message ack_msg = init_message(NULL, ACK, 0);
        if (send(self, 0, &ack_msg) < 0) {
            perror("send error");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}

int children_slave_work(struct SystemInfo * systemInfo) {
    log_pa(Event, log_started_fmt, get_physical_time(), currentProcId, getpid(), getppid(), local_balance);
    char start_str[256];
    sprintf(start_str, log_started_fmt, get_physical_time(), currentProcId, getpid(), getppid(), local_balance);
    const Message msg = init_message(start_str, STARTED, strlen(start_str));
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
    log_pa(Event, log_received_all_started_fmt, get_physical_time(), currentProcId);
    //some work here

    int running = 0;
    while (running == 0) {
        Message msg;
        if (receive_any(systemInfo, &msg) < 0) {
            perror("receive all err");
            exit(EXIT_FAILURE);
        }
        if (msg.s_header.s_magic != MESSAGE_MAGIC) {
            perror("message magic not right");
            exit(EXIT_FAILURE);
        }
        switch (msg.s_header.s_type) {
            case TRANSFER:
                transfer_work(systemInfo, &msg);
                break;
            case STOP:
                running = 1;
                break;
        }
    }

    //send done message
    log_pa(Event, log_done_fmt, get_physical_time(), currentProcId, local_balance);
    char end_str[256];
    sprintf(end_str, log_done_fmt, get_physical_time(), currentProcId, local_balance);
    const Message done_msg = init_message(end_str, DONE, strlen(end_str));
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
    log_pa(Event, log_received_all_done_fmt, get_physical_time(), currentProcId);
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
    log_pa(Event, log_received_all_started_fmt, get_physical_time(), currentProcId);
    //parent work starts here

    bank_robbery(&systemInfo, systemInfo.proc_count - 1);

    const Message msg = init_message(NULL, STOP, 0);
    if (send_multicast(&systemInfo, &msg) != 0) {
        return -1;
    }

    //parent work ends here
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
    log_pa(Event, log_received_all_done_fmt, get_physical_time(), currentProcId);
    return 0;
}

int close_unreachable_pipes(struct SystemInfo cur_sys_info){
    for (local_id i = 0; i < cur_sys_info.proc_count; i++) {
        for (local_id j = 0; j < cur_sys_info.proc_count; j++) {
            if (i == currentProcId) {
                if (cur_sys_info.proccessesInfo[i].pipes[j][0] != 1) {
                    close(cur_sys_info.proccessesInfo[i].pipes[j][0]);
                }
                continue;
            } else if (j == currentProcId) {
                if (cur_sys_info.proccessesInfo[i].pipes[j][1] != 1) {
                    close(cur_sys_info.proccessesInfo[i].pipes[j][1]);
                }
                continue;
            }
            if (cur_sys_info.proccessesInfo[i].pipes[j][1] != 1) {
                close(cur_sys_info.proccessesInfo[i].pipes[j][1]);
            }
                if (cur_sys_info.proccessesInfo[i].pipes[j][0] != 1) {
                    close(cur_sys_info.proccessesInfo[i].pipes[j][0]);
                }
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
            if (fcntl(systemInfo.proccessesInfo[i].pipes[j][0], F_SETFL, O_NONBLOCK) < 0) {
                perror("fcntl 0 error");
                exit(EXIT_FAILURE);
            }
            if (fcntl(systemInfo.proccessesInfo[i].pipes[j][1], F_SETFL, O_NONBLOCK) < 0) {
                perror("fcntl 1 error");
                exit(EXIT_FAILURE);
            }
            log_pa(Pipe, "i%d/o%d\n",
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
                local_balance = atoi(argv[i + 2]);
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
