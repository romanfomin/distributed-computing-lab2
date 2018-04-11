#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "lab1.h"
#include "banking.h"

SelfStruct* create_self_struct(int*** matrix, local_id proc_numb, int N, int log_fd){
	SelfStruct* selfStruct = (SelfStruct*)malloc(sizeof(SelfStruct));
	selfStruct->N = N;
	selfStruct->fd_matrix = matrix;
	selfStruct->src = proc_numb;
	selfStruct->log_fd = log_fd;

	return selfStruct;
}

Options* get_arg(int argc, char* argv[]){
	int value, i;
	Options* opts = (Options*)malloc(sizeof(Options));
	while((value = getopt(argc,argv,"p:"))!=-1){
		switch(value){
			case 'p':
			opts->N = atoi(optarg);
			for(i = 0; i < opts->N && optind<argc; i++){
				opts->values[i] = atoi(argv[optind++]);
			}
		}
	}
	return opts;
}

int receive_messages(MessageType msg_type, local_id proc_numb, int*** matrix, int N, int log_fd){
	int i;
	SelfStruct* selfStruct = (SelfStruct*)malloc(sizeof(SelfStruct));
	Message* msg = (Message*)malloc(sizeof(Message));
	char buf[100];

	selfStruct->N = N;
	selfStruct->fd_matrix = matrix;
	selfStruct->src = proc_numb;
	selfStruct->log_fd = log_fd;

	switch(msg_type){
		case STARTED:
			sprintf(buf, log_received_all_started_fmt, get_physical_time(), proc_numb);
			break;
		case DONE:
			sprintf(buf, log_received_all_done_fmt, get_physical_time(), proc_numb);
			break;
		default:
			break;
	}

	for(i = 0; i < N; i++){
		if((i + 1) == proc_numb){
			continue;
		}
		if(receive(selfStruct, i + 1, msg) == -1){
			return -1;
		}
		if(!msg->s_header.s_type == msg_type){
			return -1;
		}
	}
	if(write_to_events_log(log_fd, buf, strlen(buf)) == -1){
		return -1;
	}
	return 0;
}

int receive(void * self, local_id from, Message * msg){
	SelfStruct* selfStruct = (SelfStruct*)self;
	TransferOrder* transferOrder;
	int fd;
	local_id to;
	int*** matrix;
	char buf[100];

	to = selfStruct->src;
	matrix = selfStruct->fd_matrix;
	fd = matrix[from][to][0];

	// printf("Process %i waits for message from process %i\n", to, from);
	if(read(fd, msg, sizeof(Message)) == -1){
		return -1;
	}
	
	switch(msg->s_header.s_type){
		case TRANSFER:
			transferOrder=(TransferOrder*)msg->s_payload;
			sprintf(buf, log_transfer_in_fmt, get_physical_time(), to, transferOrder->s_amount, from);
			if(from != 0){
				if(write_to_events_log(selfStruct->log_fd, buf, strlen(buf)) == -1){
					return -1;
				}
			}
			break;
		default:
			break;
	}
	// printf("Process %i received message from process %i\n", to, from);
	return 0;
}


int fd_is_valid(int fd){
	return fcntl(fd, F_GETFD) != -1;
}

int write_to_events_log(int fd, char* buf, int length){
	if(write(fd, buf, length) == -1){
		printf("Error: cannot write to %s\n", events_log);
		return -1;
	}
	printf("%s", buf);
	return 0;
}

int send_messages(MessageType msg_type, local_id proc_numb, int*** matrix, int N, int log_fd, balance_t balance){
	Message* msg;
	SelfStruct* selfStruct = (SelfStruct*)malloc(sizeof(SelfStruct));
	char buf[100];


	switch(msg_type){
		case STARTED:
			sprintf(buf, log_started_fmt, get_physical_time(), proc_numb, getpid(), getppid(), balance);
			break;
		case DONE:
			sprintf(buf, log_done_fmt, get_physical_time(), proc_numb, balance);
			break;
		default:
			break;
	}

	msg = create_message(msg_type, buf, strlen(buf));
	selfStruct->N = N;
	selfStruct->fd_matrix = matrix;
	selfStruct->src = proc_numb;
	selfStruct->log_fd = log_fd;
	if(send_multicast(selfStruct, msg) == -1){
		return -1;
	}
	return 0;
}

Message* create_message(MessageType msg_type, char* buf, int length){
	MessageHeader* header=(MessageHeader*)malloc(sizeof(MessageHeader));
	Message* msg = (Message*)malloc(sizeof(Message));
	header->s_magic = MESSAGE_MAGIC;
	header->s_payload_len = length;
	header->s_type = msg_type;
	header->s_local_time = get_physical_time();

	msg->s_header = *header;
	memcpy(msg->s_payload, buf, length);
	return msg;
}

int send(void * self, local_id dst, const Message * msg){
	SelfStruct* selfStruct = (SelfStruct*)self;
	int fd;
	local_id src;
	int*** matrix;
	TransferOrder* transferOrder;
	char buf[100];

	src = selfStruct->src;
	matrix = selfStruct->fd_matrix;
	fd = matrix[src][dst][1];

	switch(msg->s_header.s_type){
		case TRANSFER:
			transferOrder=(TransferOrder*)msg->s_payload;
			sprintf(buf, log_transfer_out_fmt, get_physical_time(), src, transferOrder->s_amount, dst);
			if(src!=0){
				if(write_to_events_log(selfStruct->log_fd, buf, strlen(buf)) == -1){
					return -1;
				}
			}
			break;
		default:
			break;
	}

	if(!fd_is_valid(fd)){
		printf("Error: invalid fd: %i\n", fd);
		return -1;
	}

	if(write(fd, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) == -1){
		printf("Error: cannot write to: %i\n", fd);
		return -1;
	}
	return 0;
}

int send_multicast(void * self, const Message * msg){
	int i;
	SelfStruct* selfStruct = (SelfStruct*)self;
	local_id src;
	int N;

	src = selfStruct->src;
	N = selfStruct->N;

	for(i = 0; i <= N; i++){
		if(i == src){
			continue;
		}
		if(write_to_events_log(selfStruct->log_fd, (char*)msg->s_payload, msg->s_header.s_payload_len) == -1){
			return -1;
		}
		if(send(self, i, msg) == -1){
			return -1;
		}

	}
	return 0;
}

int*** create_matrix(int N){
	int i, j;
	int *** matrix = NULL;

	matrix = (int***)malloc((N + 1) * sizeof(int**));
	for(i = 0; i <= N; i++){
		matrix[i]=(int**)malloc((N + 1) * sizeof(int*));
		for(j = 0; j <= N; j++){
			matrix[i][j] = (int*)malloc(2 * sizeof(int));
		}
	}
	return matrix;
}

int fill_matrix(int*** matrix, int N){
	int i, j;
	char buf[100];
	int fd[2];
	int log_fd;

	if((log_fd = open(pipes_log, O_WRONLY)) == -1){
		printf("Error: cannot open file %s.\n", pipes_log);
		return -1;
	}

	for(i = 0; i <= N; i++){
		for(j = 0; j <= N; j++){
			if(i == j){
				continue;
			}
			if(pipe(fd) == -1){
				printf("Error: cannot create pipe.\n");
				return -1;
			}
			matrix[i][j][0]=fd[0];
			matrix[i][j][1]=fd[1];
			sprintf(buf,"Pipe %i - %i created, fd0(r) = %i fd1(w) = %i\n", i, j, fd[0], fd[1]);
			if(write(log_fd, buf, strlen(buf)) == -1){
				printf("Error: cannot write to %s\n", pipes_log);
				return -1;
			}
		}
	}
	return 0;
}

int close_fd(int fd){
	if(close(fd) == -1){
		printf("Error: cannot close pipe %i.\n", fd);
		return -1;
	}
	return 0;
}

int close_unneccessary_fd(int*** matrix, int N, int proc_numb){
	int i, j, k;

	for(i = 0; i <= N; i++){
		for(j = 0; j <= N; j++){
			if(i == j){
				continue;
			}
			if(i == proc_numb){
				if(close_fd(matrix[i][j][0]) == -1){
					return -1;
				}
				continue;
			}
			if(j == proc_numb){
				if(close_fd(matrix[i][j][1]) == -1){
					return -1;
				}
				continue;
			}
			for(k = 0; k < 2; k++){
				if(close_fd(matrix[i][j][k]) == -1){
					return -1;
				}
			}
		}
	}
	return 0;
}
