#ifndef _LAB1_H
#define _LAB1_H

typedef struct{
	int*** fd_matrix;
	local_id src;
	int N;
	int log_fd;
} SelfSend;

typedef struct{
	MessageType msg_type;
	int*** fd_matrix;
	local_id to;
	int N;
	int log_fd;
} SelfRecieve;

typedef struct{
	int N;
	int values[10];
} Options;

Options* get_arg(int argc, char* argv[]);
int*** create_matrix(int N);
int fill_matrix(int*** matrix, int N);
int close_unneccessary_fd(int*** matrix, int N, int proc_numb);
int close_fd(int fd);
int fd_is_valid(int fd);
int send_messages(MessageType msg_type, local_id proc_numb, int*** matrix, int N, int log_fd);
Message* create_message(MessageType msg_type, char* buf, int length);
int write_to_events_log(int fd, char* buf, int length);
int receive_messages(MessageType msg_type, local_id proc_numb, int*** matrix, int N, int log_fd);

#endif
