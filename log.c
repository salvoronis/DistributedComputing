#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "common.h"
#include "log.h"
#include "io.h"
#include "pa1.h"
#include "proc_info.h"
#include "ipc.h"

static FILE *eventLogFile = NULL;
static FILE *pipesLogFile = NULL;

void init_log(){
    eventLogFile = fopen(events_log, "a");
    pipesLogFile = fopen(pipes_log, "a");
}

void close_log(){
    fclose(eventLogFile);
    fclose(pipesLogFile);
}

const char *message_type_str(MessageType type){
    switch (type){
        case STARTED:
            return "STARTED";
        case DONE:
            return "DONE";
        case ACK:
            return "ACK";
        case STOP:
            return "STOP";
        case TRANSFER:
            return "TRANSFER";
        case BALANCE_HISTORY:
            return "BALANCE_HISTORY";
//        case CS_REQUEST:
//            break;
//        case CS_REPLY:
//            break;
//        case CS_RELEASE:
//            break;
        default:
            return "UNKNOWN";
    }
}

void log_pa(LogType type, const char *format, int argc_count, ...){
    va_list list;
    va_start(list, argc_count);

    switch (type)
    {
        case Event:
        {
            vprintf(format, list);
            char logStr[256];
            memset(logStr, 0, sizeof(logStr));
            vsnprintf(logStr, 256, format, list);
            fwrite(logStr, sizeof(char), strlen(logStr), eventLogFile);
            break;
        }
        case Pipe:
        {
            char logStr[256];
            memset(logStr, 0, sizeof(logStr));
            vsnprintf(logStr, 256, format, list);
            fwrite(logStr, sizeof(char), strlen(logStr), pipesLogFile);
            break;
        }
        case Info:
        {
            Message *message = va_arg(list, Message*);
            printf("Message info:\n");
            printf("\ts_magic: 0x%X\n", message->s_header.s_magic);
            printf("\ts_payload_len: %d\n", message->s_header.s_payload_len);
            printf("\ts_type: %s\n", message_type_str(message->s_header.s_type));
            printf("\ts_local_time: %d\n", message->s_header.s_local_time);
            printf("\ts_payload: %s\n", message->s_payload);
            break;
        }
        default:
        {
            vprintf(format, list);
            break;
        }
    }

    va_end(list);
}
