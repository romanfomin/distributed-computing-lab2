#ifndef _LAB1_H
#define _LAB1_H

#include "banking.h"

typedef struct{
	int*** fd_matrix;
	local_id src;
	int N;
	int log_fd;
} SelfStruct;

typedef struct{
	int N;
	int values[10];
} Options;

SelfStruct* create_self_struct(int*** matrix, local_id proc_numb, int N, int log_fd);
Options* get_arg(int argc, char* argv[]);
int*** create_matrix(int N);
int fill_matrix(int*** matrix, int N);
int close_unneccessary_fd(int*** matrix, int N, int proc_numb);
int close_fd(int fd);
int fd_is_valid(int fd);
int send_messages(MessageType msg_type, local_id proc_numb, int*** matrix, int N, int log_fd, balance_t balance);
Message* create_message(MessageType msg_type, char* buf, int length);
int write_to_events_log(int fd, char* buf, int length);
int receive_messages(MessageType msg_type, local_id proc_numb, int*** matrix, int N, int log_fd);

#endif
