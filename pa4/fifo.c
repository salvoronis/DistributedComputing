//
// Created by salvoroni on 1/6/22.
//

#include "fifo.h"
#include <malloc.h>
#include <string.h>

typedef struct linked_list {
    int requested_pid_id;
    struct linked_list * next;
    struct linked_list * prev;
} linked_list;

static linked_list *fifo;

void push(int pid){
    if(fifo == NULL) {
        fifo = (linked_list*) malloc(sizeof(linked_list));
        fifo->requested_pid_id = pid;
        fifo->next = NULL;
        fifo->prev = NULL;
        return;
    }
    linked_list * tmp = (linked_list*) malloc(sizeof(linked_list));
    tmp->requested_pid_id = pid;
    tmp->next = fifo;
    tmp->prev = NULL;
    fifo->prev = tmp;
    fifo = tmp;
}

int is_empty() {
    return fifo == NULL;
}

int pop() {
    linked_list * tmp = fifo;
    while (tmp->next != NULL) {
        tmp = tmp->next;
    }
    int val = tmp->requested_pid_id;
    linked_list * prev_tmp = tmp->prev;
    if(prev_tmp != NULL) {
//        free(prev_tmp->next);
        prev_tmp->next = NULL;
    } else {
        fifo = NULL;
    }
//    free(tmp);
//    tmp = NULL;
//    fifo = prev_tmp;
    return val;
}
