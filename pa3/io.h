#ifndef PA1_IO_H
#define PA1_IO_H

Message init_message(void* data, MessageType type, size_t data_len);
int receive_blocking(void * self, local_id id, Message * msg);

#endif //PA1_IO_H
