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
#include "pa2345.h"
#include "lab1.h"

int main(int argc, char** argv){

	int i, N;
	pid_t pid;
	local_id local_proc_id;
	int*** fd_matrix;
	int log_fd;
	Options* opts=(Options*)malloc(sizeof(Options));
	

	if((log_fd = open(events_log, O_WRONLY)) == -1){
		printf("Error: cannot open file %s.\n", events_log);
		exit(1);
	}

	local_proc_id = PARENT_ID;


	opts=get_arg(argc,argv);
	N=opts->N;

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

				send_messages(STARTED, local_proc_id, fd_matrix, N, log_fd);
				receive_messages(STARTED, local_proc_id, fd_matrix, N, log_fd);



				send_messages(DONE, local_proc_id, fd_matrix, N, log_fd);
				receive_messages(DONE, local_proc_id, fd_matrix, N, log_fd);

				exit(0);
			default:
				break;
		}
	}

	if(close_unneccessary_fd(fd_matrix, N, PARENT_ID) == -1){
		exit(1);
	}

	receive_messages(STARTED, local_proc_id, fd_matrix, N, log_fd);

	receive_messages(DONE, local_proc_id, fd_matrix, N, log_fd);

	for (i = 0; i < N; i++){
		wait(&pid);
	}

	return 0;
}
