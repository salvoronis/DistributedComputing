//
// Created by salvoroni on 1/6/22.
//
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "ipc.h"
#include "io.h"
#include "pa2345.h"

int request_cs(const void * self) {
    const Message req_msg = init_message(NULL, CS_REQUEST, 0);
    if (send((void*)self, 0, &req_msg) < 0) {
        perror("send error");
        exit(EXIT_FAILURE);
    }
    Message res_msg;
    if (receive_blocking((void*)self, 0, &res_msg) != 0) {
        perror("transfer error");
        exit(-1);
    }
    return 0;
}

int release_cs(const void * self) {
    const Message release_msg = init_message(NULL, CS_RELEASE, 0);
    if (send((void*)self, 0, &release_msg) < 0) {
        perror("send error");
        exit(EXIT_FAILURE);
    }
    return 0;
}
