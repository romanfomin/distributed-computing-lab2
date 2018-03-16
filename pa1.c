#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

int close_fd(int fd);
int close_unneccessary_fd(int** fd, int N, int local_proc_id);
int fd_is_valid(int fd);
int get_arg(int argc, char** argv);
int fill_fd_matrix(int** fd_w, int** fd_r, int N);
int** create_matrix(int N);
int swap_parts(int** fd_w, int** fd_r, int N);
int create_pipe(int** fd, int i, int j, int pfd[2],int log_fd);

int main(int argc, char** argv){

	pid_t pid;
	int i, N;
	int** fd_write = NULL;
	int** fd_read = NULL;
	local_id local_proc_id;

	local_proc_id = PARENT_ID;

	if((N = get_arg(argc, argv)) == -1){
		exit(1);
	}
	
	fd_write = create_matrix(N);
	fd_read = create_matrix(N);

	if((fill_fd_matrix(fd_write, fd_read, N)) == -1){
		exit(1);
	}

	for (i = 0; i < N; i++){
		switch(pid = fork()){
			case -1:
				printf("Error\n");
				break;
			case 0:
				printf("Child pid=%i ppid=%i\n",getpid(),getppid());
				local_proc_id = i + 1;
				close_unneccessary_fd(fd_write, N, local_proc_id);
				close_unneccessary_fd(fd_read, N, local_proc_id);

				exit(0);
			default:
				printf("Parent\n");
				break;
		}
	}

	close_unneccessary_fd(fd_write, N, PARENT_ID);
	close_unneccessary_fd(fd_read, N, PARENT_ID);

	for (i = 0; i < N; i++){
		printf("Waiting...\n");
		wait(&pid);
	}

	// int j;
	// for(i = 0; i < N + 1; i++){
	// 	for(j = 0; j < N + 1; j++){
	// 		if(i == j){
	// 			printf("x ");
	// 		}
	// 		else{
	// 			printf("%i ", fd_write[i][j]);
	// 		}

	// 		// else if(fd_is_valid(fd[i][j])){
	// 		// 	printf("+");
	// 		// }else{
	// 		// 	printf("-");
	// 		// }
	// 	}
	// 	printf("\n");
	// }

	printf("End.\n");
	return 0;
}

int proc_start(){

	return 0;
}

int** create_matrix(int N){
	int** fd;
	int i;
	fd = (int**)malloc((N + 1) * sizeof(int*));
	for (i = 0; i <= N; i++){
		fd[i] = (int*)malloc((N + 1) * sizeof(int));
	}
	return fd;
}

int create_pipe(int** fd, int i, int j, int pfd[2], int log_fd){
	char buf[100];

	if(pipe(pfd) == -1){
		printf("Error: cannot create pipe.\n");
		return -1;
	}
	sprintf(buf,"Pipe %i - %i created, fd0(r) = %i fd1(w) = %i\n", i, j, pfd[0], pfd[1]);
	if(write(log_fd, buf, strlen(buf)) == -1){
		printf("Error: cannot write to %s\n", pipes_log);
		return -1;
	}
	fd[i][j] = pfd[1];
	fd[j][i] = pfd[0];
	return 0;
}

int fill_fd_matrix(int** fd_w, int** fd_r, int N){
	int i, j;
	int pipe_fd[2];
	int log_fd;
	
	if((log_fd = open(pipes_log, O_WRONLY)) == -1){
		printf("Error: cannot open file %s.\n", pipes_log);
		return -1;
	}

	for(i = 0; i < N + 1; i++){
		for(j = i + 1; j < N + 1; j++){
			if(create_pipe(fd_w, i, j, pipe_fd, log_fd) == -1){
				return -1;
			}

			if(create_pipe(fd_r, i, j, pipe_fd, log_fd) == -1){
				return -1;
			}
		}
	}

	swap_parts(fd_w, fd_r, N);
	return 0;
}

int swap_parts(int** fd_w, int** fd_r, int N){
	int i, j;
	int t;
	for(i = 0; i < N + 1; i++){
		for(j = i + 1; j < N + 1; j++){
			t = fd_w[j][i];
			fd_w[j][i] = fd_r[i][j];
			fd_r[i][j] = t;
		}
	}
	return 0;
}

int get_arg(int argc, char** argv){
	int N;
	if(argc < 3 || strcmp(argv[1],"-p") != 0 || (N = atoi(argv[2])) < 0){
		printf("Wrong arguments.\nUsage:\n\tpa1 [-p number] (number > 0)\n");
		return -1;
	}
	return N;
}

int fd_is_valid(int fd){
	return fcntl(fd, F_GETFD) != -1;
}

int close_fd(int fd){
	if(close(fd) == -1){
		printf("Error: cannot close pipe %i.\n", fd);	
		return -1;
	}
	return 0;
}

int close_unneccessary_fd(int** fd, int N, int local_proc_id){
	int i, j;
	for (i = 0; i < N + 1; i ++){
		for (j = 0; j < N + 1; j ++){
			if(i == j){
				continue;
			}
			if(i == local_proc_id || j == local_proc_id){
				continue;
			}
			
			close_fd(fd[i][j]);
		}
	}
	return 0;
}
