#include "banking.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "ipc.h"
#include "proc_info.h"
#include "io.h"
#include "log.h"
#include "pa2345.h"
#include "lamport_utils.h"

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
    if (receive_blocking(system_info, dst, &ack_msg) != 0) {
        perror("transfer error");
        exit(-1);
    }
}

local_id currentProcId = -1;
balance_t local_balance = 0;

int transfer_work(void * self, Message * msg, BalanceHistory *history) {
    TransferOrder * transfer = (TransferOrder*) msg->s_payload;
    if (currentProcId == transfer->s_src) {
        local_balance -= transfer->s_amount;
        log_pa(Event, log_transfer_out_fmt, get_lamport_time(), transfer->s_src, transfer->s_amount, transfer->s_dst);
        /*for (timestamp_t i = msg->s_header.s_local_time - 1; i < get_lamport_time(); i++) {
            history->s_history[i].s_balance -= transfer->s_amount;
        }*/
        if (send(self, transfer->s_dst, msg) < 0) {
            perror("send error");
            exit(EXIT_FAILURE);
        }
    } else if (currentProcId == transfer->s_dst) {
        local_balance += transfer->s_amount;
        log_pa(Event, log_transfer_in_fmt, get_lamport_time(), transfer->s_dst, transfer->s_amount, transfer->s_src);

        for (timestamp_t i = msg->s_header.s_local_time - 1; i < get_lamport_time(); i++) {
            history->s_history[i].s_balance_pending_in += transfer->s_amount;
        }

        const Message ack_msg = init_message(NULL, ACK, 0);
        if (send(self, 0, &ack_msg) < 0) {
            perror("send error");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}

int children_slave_work(struct SystemInfo * systemInfo) {
    log_pa(Event, log_started_fmt, get_lamport_time(), currentProcId, getpid(), getppid(), local_balance);
    char start_str[256];
    sprintf(start_str, log_started_fmt, get_lamport_time(), currentProcId, getpid(), getppid(), local_balance);
    const Message msg = init_message(start_str, STARTED, strlen(start_str));
    if (send_multicast(systemInfo, &msg) != 0) {
        return -1;
    }
    Message messages_tmp;
    for (local_id i = 1; i < systemInfo->proc_count; i++) {
        if (currentProcId == i) {continue;}
        if (receive_blocking(systemInfo, i, &messages_tmp) != 0) {
            return -1;
        }
        if (messages_tmp.s_header.s_magic != MESSAGE_MAGIC &&
            messages_tmp.s_header.s_type != STARTED) {
            return -1;
        }
    }
    log_pa(Event, log_received_all_started_fmt, get_lamport_time(), currentProcId);
    //some work here

    int running = 0;
    BalanceHistory history;
    history.s_id = currentProcId;
    history.s_history_len = 0;
    while (running == 0) {
        Message msg;
        if (receive_any(systemInfo, &msg) < 0) {
            perror("receive all err");
            exit(EXIT_FAILURE);
        }
        timestamp_t now = get_lamport_time();
        for (timestamp_t i = history.s_history_len; i < now; i++) {
            history.s_history[i].s_time = i;
            history.s_history[i].s_balance = local_balance;
            history.s_history[i].s_balance_pending_in = 0;
        }
        history.s_history_len = now;
        if (msg.s_header.s_magic != MESSAGE_MAGIC) {
            perror("message magic not right");
            exit(EXIT_FAILURE);
        }
        switch (msg.s_header.s_type) {
            case TRANSFER:
                transfer_work(systemInfo, &msg, &history);
                break;
            case STOP:
                running = 1;
                break;
        }
    }

    history.s_history[history.s_history_len].s_time = history.s_history_len;
    history.s_history[history.s_history_len].s_balance = local_balance;
    history.s_history[history.s_history_len].s_balance_pending_in = 0;
    history.s_history_len++;

    //send done message
    log_pa(Event, log_done_fmt, get_lamport_time(), currentProcId, local_balance);
    char end_str[256];
    sprintf(end_str, log_done_fmt, get_lamport_time(), currentProcId, local_balance);
    const Message done_msg = init_message(end_str, DONE, strlen(end_str));
    if (send_multicast(systemInfo, &done_msg) != 0) {
        return -1;
    }
    Message messages_done_tmp;
    for (local_id i = 1; i < systemInfo->proc_count; i++) {
        if (currentProcId == i) {continue;}
        if (receive_blocking(systemInfo, i, &messages_done_tmp) != 0) {
            return -1;
        }
        if (messages_done_tmp.s_header.s_magic != MESSAGE_MAGIC &&
            messages_done_tmp.s_header.s_type != DONE) {
            return -1;
        }
    }
    log_pa(Event, log_received_all_done_fmt, get_lamport_time(), currentProcId);

//    inc_lamport();
    Message balance_history_msg = init_message(&history, BALANCE_HISTORY, offsetof(BalanceHistory, s_history) + sizeof(BalanceState) * history.s_history_len);
    if (send(systemInfo, 0, &balance_history_msg) != 0) {
        perror("balance history error");
        exit(-1);
    }
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
        if (receive_blocking(&systemInfo, i, &messages_started_tmp) != 0) {
            return -1;
        }
        if (messages_started_tmp.s_header.s_magic != MESSAGE_MAGIC &&
            messages_started_tmp.s_header.s_type != STARTED) {
            return -1;
        }
    }
    log_pa(Event, log_received_all_started_fmt, get_lamport_time(), currentProcId);
    //parent work starts here

    bank_robbery(&systemInfo, systemInfo.proc_count - 1);

    const Message msg = init_message(NULL, STOP, 0);
    if (send_multicast(&systemInfo, &msg) != 0) {
        return -1;
    }

    //parent work ends here
    Message messages_done_tmp;
    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        if (receive_blocking(&systemInfo, i, &messages_done_tmp) != 0) {
            return -1;
        }
        if (messages_done_tmp.s_header.s_magic != MESSAGE_MAGIC &&
            messages_done_tmp.s_header.s_type != DONE) {
            return -1;
        }
    }
    log_pa(Event, log_received_all_done_fmt, get_lamport_time(), currentProcId);

    Message messages_balance_history_tmp;
    AllHistory allHistory;
    allHistory.s_history_len = systemInfo.proc_count - 1;
    int max_len = 0;
    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        if (receive_blocking(&systemInfo, i, &messages_balance_history_tmp) != 0) {
            return -1;
        }
        if (messages_balance_history_tmp.s_header.s_magic != MESSAGE_MAGIC &&
                messages_balance_history_tmp.s_header.s_type != BALANCE_HISTORY) {
            return -1;
        }
        BalanceHistory *childrenHistory = (BalanceHistory*) messages_balance_history_tmp.s_payload;
        allHistory.s_history[i - 1].s_id = childrenHistory->s_id;
        allHistory.s_history[i - 1].s_history_len = childrenHistory->s_history_len;
        if (childrenHistory->s_history_len > max_len) {
            max_len = childrenHistory->s_history_len;
        }
        for (int h = 0; h < childrenHistory->s_history_len; h++) {
            allHistory.s_history[i - 1].s_history[h] = childrenHistory->s_history[h];
        }
    }
    //fill history
    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        for (int h = 0; h < max_len; h++) {
            if (allHistory.s_history[i - 1].s_history[h].s_balance == 0) {
                allHistory.s_history[i - 1].s_history_len++;
                allHistory.s_history[i - 1].s_history[h] = allHistory.s_history[i - 1].s_history[h-1];
                allHistory.s_history[i - 1].s_history[h].s_time++;
            }
        }
    }
    print_history(&allHistory);
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
            if (cur_sys_info.proccessesInfo[i].pipes[j][1] != -1) {
                close(cur_sys_info.proccessesInfo[i].pipes[j][1]);
            }
            if (cur_sys_info.proccessesInfo[i].pipes[j][0] != -1) {
                close(cur_sys_info.proccessesInfo[i].pipes[j][0]);
            }
        }
    }
    return 0;
}

void dump_args(int argc, char* argv[]) {
    printf("argv:");

    for (int i = 0; i < argc; ++i) {
        printf(" %p", (void*) argv[i]);
    }

    printf("\n");
}

int main(int argc, char *argv[]) {
    init_log();
    struct SystemInfo systemInfo;
    memset(&systemInfo, 0, sizeof(systemInfo));
    systemInfo.proccessesInfo[0].proc_pid = getpid();
    systemInfo.proc_count = atoi(argv[2]) + 1;

//    dump_args(argc, argv);
    // 0 process is always main
    for(int i = 0; i < 11; i++) {
        for(int j = 0; j < 11; j++) {
            systemInfo.proccessesInfo[i].pipes[j][0] = -2;
            systemInfo.proccessesInfo[i].pipes[j][1] = -2;
//            dump_args(argc, argv);
        }
    }
    for (int i = 0; i < systemInfo.proc_count; i++) {
        for (int j = 0; j < systemInfo.proc_count; j++) {
            if (i == j) {continue;}
            if (pipe(systemInfo.proccessesInfo[i].pipes[j]) == -1) {
                perror("pipe error");
                exit(EXIT_FAILURE);
            }
            int flag = fcntl(systemInfo.proccessesInfo[i].pipes[j][0], F_GETFL);
            if (fcntl(systemInfo.proccessesInfo[i].pipes[j][0], F_SETFL, flag | O_NONBLOCK) < 0) {
                perror("fcntl 0 error");
                exit(EXIT_FAILURE);
            }
            flag = fcntl(systemInfo.proccessesInfo[i].pipes[j][1], F_GETFL);
            if (fcntl(systemInfo.proccessesInfo[i].pipes[j][1], F_SETFL, flag | O_NONBLOCK) < 0) {
                perror("fcntl 1 error");
                exit(EXIT_FAILURE);
            }
            log_pa(Pipe, "i%d/o%d\n",
                   systemInfo.proccessesInfo[i].pipes[j][1],
                   systemInfo.proccessesInfo[i].pipes[j][0]);
        }
    }
//    systemInfo.proccessesInfo[0].proc_pid = getpid();
//    systemInfo.proc_count = atoi(argv[2]) + 1;
    for (local_id i = 1; i < systemInfo.proc_count; i++) {
        systemInfo.proccessesInfo[i].proc_pid = fork();
        switch (systemInfo.proccessesInfo[i].proc_pid) {
            case -1: {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            case 0: {
                //start proc
                local_balance = atoi(argv[i + 2]);
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
