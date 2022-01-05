#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include "ipc.h"
#include "proc_info.h"
#include "log.h"
#include "banking.h"
#include "lamport_utils.h"

extern local_id currentProcId;

Message init_message(void* data, MessageType type, size_t data_size) {
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    if (data_size == 0) {
        msg.s_header.s_payload_len = 0;
    } else {
        msg.s_header.s_payload_len = data_size;
        memcpy(msg.s_payload, data, msg.s_header.s_payload_len);
    }
    msg.s_header.s_type = type;
    msg.s_header.s_local_time = get_lamport_time();
    return msg;
}

int send_all(int to, void * buffer, ssize_t expected_size) {
    uint8_t * ptr = buffer;

    if (expected_size == 0) {
        return 0;
    }

    while (1) {
        ssize_t amount = write(to, ptr, expected_size);
        switch (amount) {
            case -1:
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    sched_yield();
                    sleep(5);
                    break;
                } else {
                    return -1;
                }
            case 0:
                return 0;
            default:
                expected_size -= amount;
                ptr += amount;
                if (expected_size != 0) {
                    //сообщение еще не ушло полностью
                    break;
                } else {
                    return 0;
                }
        }
    }
}

static void fucking_const_msg(const Message * msg, Message * new_msg) {
    new_msg->s_header = msg->s_header;
    memcpy(new_msg->s_payload, msg->s_payload, msg->s_header.s_payload_len);
}

static int do_send(void * self, local_id dst, const Message * msg) {
    struct SystemInfo * sysInfo = (struct SystemInfo *) self;
    if (currentProcId == -1) {
        return -1;
    }
    log_pa(Pipe, "%d -to> %d|write%d/%d\n",
           currentProcId,
           dst,
           sysInfo->proccessesInfo[currentProcId].pipes[dst][1],
           sysInfo->proccessesInfo[currentProcId].pipes[dst][0]);

    ssize_t res = send_all(
            sysInfo->proccessesInfo[currentProcId].pipes[dst][1],
            (void *) msg,
            sizeof(MessageHeader) + msg->s_header.s_payload_len);
    if (res == -1) {
        return -1;
    }
    return 0;
}

int send(void * self, local_id dst, const Message * msg) {
    inc_lamport();
    Message normal_message;
    fucking_const_msg(msg, &normal_message);
    normal_message.s_header.s_local_time = get_lamport_time();
    struct SystemInfo * sysInfo = (struct SystemInfo *) self;
    if (currentProcId == -1) {
        return -1;
    }
    log_pa(Pipe, "%d -to> %d|write%d/%d\n",
           currentProcId,
           dst,
           sysInfo->proccessesInfo[currentProcId].pipes[dst][1],
           sysInfo->proccessesInfo[currentProcId].pipes[dst][0]);

    ssize_t res = send_all(
            sysInfo->proccessesInfo[currentProcId].pipes[dst][1],
            (void *) &normal_message,
            sizeof(MessageHeader) + normal_message.s_header.s_payload_len);
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
    inc_lamport();
    Message normal_message;
    fucking_const_msg(msg, &normal_message);
    normal_message.s_header.s_local_time = get_lamport_time();
    for (local_id i = 0; i < sysInfo->proc_count; i ++) {
        if (i == currentProcId) {continue;}
        if (do_send(sysInfo, i, &normal_message) != 0) {
            return -1;
        }
    }
    return 0;
}

int receive_all(int from, void * buffer, ssize_t expected_size) {
    uint8_t * ptr = buffer;
    errno = 0;
    while(1) {
        ssize_t res = read(from, ptr, expected_size);
        if (res < 0) {
            if (ptr != buffer) {
                if (errno == EWOULDBLOCK || errno == EPIPE) {
                    sched_yield();
                    sleep(5);
                    continue;
                }
            }
            break;
        }

        if (res == 0) {
            if (errno == 0) {
                errno = EPIPE;
            }

            break;
        }

        expected_size -= res;
        ptr += res;

        if (expected_size == 0) {
            return 0;
        }
    }
    return -1;
}

int receive_blocking(void * self, local_id id, Message * msg) {
    while(1) {
        int res = receive(self, id, msg);
        if (res != 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                sched_yield();
//                sleep(1);
                continue;
            }
        }
        return res;
    }
}

int receive_any(void * self, Message * msg) {
    struct SystemInfo * sysInfo = (struct SystemInfo *) self;
    if (currentProcId == -1) {
        return -1;
    }
    while (1) {
        for (local_id i = 0; i < sysInfo->proc_count; ++i) {
            if (i == currentProcId) {continue;}

            if (receive(sysInfo, i, msg) == 0) {
                return 0;
            }
        }
        sched_yield();
    }
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
    if (receive_all(sysInfo->proccessesInfo[from].pipes[currentProcId][0], msg, sizeof(MessageHeader)) < 0) {
        return -1;
    }
    if (msg->s_header.s_payload_len == 0) {
        return 0;
    } else {
        while (receive_all(sysInfo->proccessesInfo[from].pipes[currentProcId][0], &msg->s_payload,
                           msg->s_header.s_payload_len) < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                sched_yield();
                continue;
            }
            return -1;
        }
    }
    if (get_lamport_time() < msg->s_header.s_local_time) {
        set_lamport(msg->s_header.s_local_time);
    }
    inc_lamport();
    return 0;
}
