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
#include "lab2.h"
#include "banking.h"

int main(int argc, char** argv){

	int i, N;
	pid_t pid;
	local_id local_proc_id;
	int*** fd_matrix;
	int log_fd;
	Options* opts=(Options*)malloc(sizeof(Options));
	BalanceHistory* balanceHistory = (BalanceHistory*)malloc(sizeof(BalanceHistory));
	AllHistory* allHistory;
	BalanceState* balanceState = (BalanceState*)malloc(sizeof(BalanceState));
	timestamp_t time;
	

	if((log_fd = open(events_log, O_WRONLY)) == -1){
		printf("Error: cannot open file %s.\n", events_log);
		exit(1);
	}

	local_proc_id = PARENT_ID;
	balance_t balance = 0;

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
				balance = opts->values[i];

				balanceState -> s_balance = balance;
				time = get_physical_time();
				balanceState -> s_time = time;
				balanceHistory->s_history[time] = *balanceState;

				if(close_unneccessary_fd(fd_matrix, N, local_proc_id) == -1){
					exit(1);
				}

				send_messages(STARTED, local_proc_id, fd_matrix, N, log_fd, balance);
				receive_messages(STARTED, local_proc_id, fd_matrix, N, log_fd);

				do_transfers(fd_matrix, local_proc_id, N, log_fd, &balance, balanceHistory);

				send_messages(DONE, local_proc_id, fd_matrix, N, log_fd, balance);
				receive_messages(DONE, local_proc_id, fd_matrix, N, log_fd);

				send_history(fd_matrix, local_proc_id, N, log_fd, balanceHistory);


				exit(0);
			default:
				break;
		}
	}

	if(close_unneccessary_fd(fd_matrix, N, PARENT_ID) == -1){
		exit(1);
	}

	receive_messages(STARTED, PARENT_ID, fd_matrix, N, log_fd);

	bank_robbery(create_self_struct(fd_matrix, PARENT_ID, N, log_fd), N);
	send_messages(STOP, PARENT_ID, fd_matrix, N, log_fd, balance);

	receive_messages(DONE, PARENT_ID, fd_matrix, N, log_fd);
	allHistory = receive_and_print_all_history(fd_matrix, PARENT_ID, N, log_fd);


	for (i = 0; i < N; i++){
		wait(&pid);
	}

	return 0;
}
