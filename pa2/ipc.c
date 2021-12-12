#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "ipc.h"
#include "proc_info.h"
#include "log.h"

extern local_id currentProcId;

Message init_message(char *data, MessageType type) {
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    if (data == NULL) {
        msg.s_header.s_payload_len = 0;
    } else {
        msg.s_header.s_payload_len = strlen(data);
    }
    msg.s_header.s_type = type;
    strncpy(msg.s_payload, data, strlen(data));
    return msg;
}

int send(void * self, local_id dst, const Message * msg) {
    struct SystemInfo * sysInfo = (struct SystemInfo *) self;
    if (currentProcId == -1) {
        return -1;
    }
    log_pa(Pipe, "%d -to> %d|write%d/%d\n",
           currentProcId,
           dst,
           sysInfo->proccessesInfo[currentProcId].pipes[dst][1],
           sysInfo->proccessesInfo[currentProcId].pipes[dst][0]);
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
    log_pa(Pipe, "%d <from- %d|read%d/%d\n",
           currentProcId,
           from,
           sysInfo->proccessesInfo[from].pipes[currentProcId][1],
           sysInfo->proccessesInfo[from].pipes[currentProcId][0]);
    ssize_t res = read(sysInfo->proccessesInfo[from].pipes[currentProcId][0], msg, sizeof(Message));
    if (res == -1) {
        return -1;
    }
    return 0;
}
