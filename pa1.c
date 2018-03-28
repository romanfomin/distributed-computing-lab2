#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

int get_arg(int argc, char** argv);
int*** create_matrix(int N);
int fill_matrix(int*** matrix, int N);
int close_unneccessary_fd(int*** matrix, int N, int proc_numb);
int close_fd(int fd);
int fd_is_valid(int fd);
int send_messages(MessageType msg_type, char* text, int text_length, local_id proc_numb, int*** matrix, int N, int log_fd);
Message* create_message(MessageType msg_type, char* buf, int length);
int write_to_events_log(int fd, char* buf, int length);
int receive_messages(MessageType msg_type, char* log_text, int text_length, local_id proc_numb, int*** matrix, int N, int log_fd);

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

int main(int argc, char** argv){

	int i, N;
	pid_t pid;
	local_id local_proc_id;
	int*** fd_matrix;
	int log_fd;
	char buf[100];
	

	if((log_fd = open(events_log, O_WRONLY)) == -1){
		printf("Error: cannot open file %s.\n", events_log);
		exit(1);
	}

	local_proc_id = PARENT_ID;

	if((N = get_arg(argc,argv)) == -1){
		exit(1);
	}

	fd_matrix = create_matrix(N);
	if(fill_matrix(fd_matrix, N) == -1){
		exit(1);
	}

	for(i = 0; i < N; i++){
		switch(pid = fork()){
			case -1:
				printf("Error\n");
				exit(1);
			case 0:
				local_proc_id = i + 1;
				if(close_unneccessary_fd(fd_matrix, N, local_proc_id) == -1){
					exit(1);
				}

				sprintf(buf, log_started_fmt, local_proc_id, getpid(), getppid());
				send_messages(STARTED, buf, strlen(buf), local_proc_id, fd_matrix, N, log_fd);
				sleep(1);
				sprintf(buf, log_received_all_started_fmt, local_proc_id);
				receive_messages(STARTED, buf, strlen(buf), local_proc_id, fd_matrix, N, log_fd);
				sleep(1);

				sprintf(buf, log_done_fmt, local_proc_id);
				send_messages(DONE, buf, strlen(buf), local_proc_id, fd_matrix, N, log_fd);
				sleep(1);
				sprintf(buf, log_received_all_done_fmt, local_proc_id);
				receive_messages(DONE, buf, strlen(buf), local_proc_id, fd_matrix, N, log_fd);
				sleep(1);

				exit(0);
			default:
				break;
		}
	}

	if(close_unneccessary_fd(fd_matrix, N, PARENT_ID) == -1){
		exit(1);
	}

	sleep(2);
	sprintf(buf, log_received_all_started_fmt, local_proc_id);
	receive_messages(STARTED, buf, strlen(buf), local_proc_id, fd_matrix, N, log_fd);
	sleep(2);

	sprintf(buf, log_received_all_done_fmt, local_proc_id);
	receive_messages(DONE, buf, strlen(buf), local_proc_id, fd_matrix, N, log_fd);
	sleep(2);

	for (i = 0; i < N; i++){
		wait(&pid);
	}

	return 0;
}


int receive_messages(MessageType msg_type, char* log_text, int text_length, local_id proc_numb, int*** matrix, int N, int log_fd){
	int i;
	SelfRecieve* selfrecieve = (SelfRecieve*)malloc(sizeof(SelfRecieve));
	Message* msg = (Message*)malloc(sizeof(Message));

	selfrecieve->N = N;
	selfrecieve->fd_matrix = matrix;
	selfrecieve->to = proc_numb;
	selfrecieve->log_fd = log_fd;
	selfrecieve->msg_type = msg_type;

	for(i = 0; i < N; i++){
		if((i + 1) == proc_numb){
			continue;
		}
		if(receive(selfrecieve, i + 1, msg) == -1){
			return -1;
		}
	}
	if(write_to_events_log(log_fd, log_text, text_length) == -1){
		return -1;
	}
	return 0;
}

int receive(void * self, local_id from, Message * msg){
	SelfRecieve* selfrecieve = (SelfRecieve*)self;
	int fd;
	local_id to;
	int*** matrix;

	to = selfrecieve->to;
	matrix = selfrecieve->fd_matrix;
	fd = matrix[from][to][0];

	// printf("Process %i waits for message from process %i\n", to, from);
	if(read(fd, msg, sizeof(Message)) == -1){
		return -1;
	}
	if(!msg->s_header.s_type == selfrecieve->msg_type){
		return -1;
	}
	// printf("Process %i received for message from process %i\n", to, from);
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
	printf("%s\n", buf);
	return 0;
}

int send_messages(MessageType msg_type, char* text, int text_length, local_id proc_numb, int*** matrix, int N, int log_fd){
	Message* msg;
	SelfSend* selfSend = (SelfSend*)malloc(sizeof(SelfSend));

	msg = create_message(msg_type, text, text_length);
	selfSend->N = N;
	selfSend->fd_matrix = matrix;
	selfSend->src = proc_numb;
	selfSend->log_fd = log_fd;
	if(send_multicast(selfSend, msg) == -1){
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
	header->s_local_time = time(NULL);

	msg->s_header = *header;
	memcpy(msg->s_payload, buf, length);
	return msg;
}

int send(void * self, local_id dst, const Message * msg){
	SelfSend* selfSend = (SelfSend*)self;
	int fd;
	local_id src;
	int*** matrix;

	src = selfSend->src;
	matrix = selfSend->fd_matrix;
	fd = matrix[src][dst][1];

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
	SelfSend* selfSend = (SelfSend*)self;
	local_id src;
	int N;

	src = selfSend->src;
	N = selfSend->N;

	for(i = 0; i <= N; i++){
		if(i == src){
			continue;
		}
		if(send(self, i, msg) == -1){
			return -1;
		}
		if(write_to_events_log(selfSend->log_fd, (char*)msg->s_payload, msg->s_header.s_payload_len) == -1){
			return -1;
		}

	}
	return 0;
}

int get_arg(int argc, char** argv){
	int N;
	if(argc!=3 || strcmp(argv[1],"-p") != 0 || (N = atoi(argv[2])) <= 0){
		printf("Wrong arguments.\nUsage:\n\tpa1 [-p number] (number > 0)\n");
		return -1;
	}
	return N;
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
