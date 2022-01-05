//
// Created by salvoroni on 1/5/22.
//

#include "banking.h"
#include "ipc.h"

static timestamp_t lamport_time = 0;

timestamp_t get_lamport_time() {
    return lamport_time;
}

void inc_lamport() {
    lamport_time++;
}

void set_lamport(timestamp_t val) {
    lamport_time = val;
}

