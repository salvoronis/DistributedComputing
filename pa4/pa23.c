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
#include <getopt.h>
#include "fifo.h"

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
int mutex_flag;

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

    char * loop_msg;
    for (int i = 1; i <= currentProcId * 5; i++) {
        loop_msg = malloc(250);
        sprintf(loop_msg, log_loop_operation_fmt, currentProcId, i, currentProcId * 5);
        if(mutex_flag) {
            //lock
            request_cs(systemInfo);
        }
        print(loop_msg);
        if(mutex_flag) {
            //unlock
            release_cs(systemInfo);
        }
        free(loop_msg);
    }

//    sleep(10);

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

    Message message_tmp;
    int next = 1;
    int done_counter = 0;
    while (1) {
        int pid = receive_any((void *)&systemInfo, &message_tmp);
        switch (message_tmp.s_header.s_type) {
            case CS_REQUEST:
                push(pid);
                break;
            case CS_RELEASE:
//                printf("released by %d\n", pid);
                next = 1;
                break;
            case DONE:
                done_counter++;
        }
        if (next == 1 && !is_empty()) {
            next = 0;
            int proc = pop();
//            printf("ok for %d\n", proc);
            const Message msg = init_message(NULL, CS_REPLY, 0);
            if (send((void*)&systemInfo, proc, &msg) < 0) {
                perror("send error");
                exit(EXIT_FAILURE);
            }
        }
        if (done_counter == systemInfo.proc_count - 1) {
            break;
        }
    }

    //parent work ends here
    log_pa(Event, log_received_all_done_fmt, get_lamport_time(), currentProcId);
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
    static struct option long_options[] = {
            {"mutexl", no_argument, &mutex_flag, 1},
    };
    int option_index = 0;
    int c;
    while((c = getopt_long(argc, argv, "p:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                systemInfo.proc_count = atoi(optarg) + 1;
        }
    }

    // 0 process is always main
    for(int i = 0; i < 11; i++) {
        for(int j = 0; j < 11; j++) {
            systemInfo.proccessesInfo[i].pipes[j][0] = -2;
            systemInfo.proccessesInfo[i].pipes[j][1] = -2;
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
    systemInfo.proccessesInfo[0].proc_pid = getpid();
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
