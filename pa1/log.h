
// Created by salvoroni on 10/14/21.
//

#ifndef PA1_LOG_H
#define PA1_LOG_H

#include "ipc.h"

typedef enum {
    Event = 0,
    Pipe,
    Info,
    Debug,
} LogType;

void init_log(void);
void close_log();
void log_pa(LogType type, const char *format, int argsAmount, ...);
void write_format_string(Message *message, const char *format, int argsAmount, ...);

#endif //PA1_LOG_H
