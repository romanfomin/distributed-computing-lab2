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
int send_start_message(local_id proc_numb, int*** matrix, int N);
Message* create_message(char* buf, int length);

typedef struct{
	int*** fd_matrix;
	local_id src;
	int N;
} SelfStruct;

int main(int argc, char** argv){

	int i, N;
	pid_t pid;
	local_id local_proc_id;
	int*** fd_matrix;
	int log_fd;
	

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
				printf("Child pid=%i ppid=%i\n",getpid(),getppid());
				local_proc_id = i + 1;
				if(close_unneccessary_fd(fd_matrix, N, local_proc_id) == -1){
					exit(1);
				}

				send_start_message(local_proc_id, fd_matrix, N);


				exit(0);
			default:
				printf("Parent\n");
				break;
		}
	}

	if(close_unneccessary_fd(fd_matrix, N, PARENT_ID) == -1){
		exit(1);
	}

	for (i = 0; i < N; i++){
		printf("Waiting...\n");
		wait(&pid);
	}


	printf("End.\n");
	return 0;
}

int fd_is_valid(int fd){
	return fcntl(fd, F_GETFD) != -1;
}

int send_start_message(local_id proc_numb, int*** matrix, int N){
	char buf[100];
	Message* msg;
	SelfStruct* selfstruct = (SelfStruct*)malloc(sizeof(SelfStruct));

	sprintf(buf, log_started_fmt,proc_numb, getpid(), getppid());
	msg = create_message(buf, strlen(buf));
	selfstruct->N = N;
	selfstruct->fd_matrix = matrix;
	selfstruct->src = proc_numb;
	if(send_multicast(selfstruct, msg) == -1){
		return -1;
	}
	return 0;
}

Message* create_message(char* buf, int length){
	MessageHeader* header=(MessageHeader*)malloc(sizeof(MessageHeader));
	Message* msg = (Message*)malloc(sizeof(Message));
	header->s_magic = MESSAGE_MAGIC;
	header->s_payload_len = length;
	header->s_type = STARTED;
	header->s_local_time = time(NULL);

	msg->s_header = *header;
	memcpy(msg->s_payload, buf, length);

	return msg;
}

int send(void * self, local_id dst, const Message * msg){
	SelfStruct* selfstruct = (SelfStruct*)self;
	int fd;
	local_id src;
	int*** matrix;

	src = selfstruct->src;
	matrix = selfstruct->fd_matrix;
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
	SelfStruct* selfstruct = (SelfStruct*)self;
	local_id src;
	int N;

	src = selfstruct->src;
	N = selfstruct->N;

	for(i = 0; i <= N; i++){
		if(i == src){
			continue;
		}
		if(send(self, i, msg) == -1){
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
