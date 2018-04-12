#include "banking.h"
#include "lab1.h"
#include "lab2.h"
#include "ipc.h"
#include <stdlib.h>
#include <stdio.h>

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount){
	Message* msg=(Message*)malloc(sizeof(Message));
	char* buf;
	TransferOrder* transferOrder = (TransferOrder*)malloc(sizeof(TransferOrder));
	transferOrder->s_src = src;
	transferOrder->s_dst = dst;
	transferOrder->s_amount = amount;

	buf = (char*) transferOrder;

	msg = create_message(TRANSFER, buf, sizeof(TransferOrder));
	if(((SelfStruct*)parent_data)->src != 0){
		((SelfStruct*)parent_data)->src=src;
		send(parent_data, dst, msg);
	}
	if(((SelfStruct*)parent_data)->src == 0){
		send(parent_data, dst, msg);
		send(parent_data, src, msg);
		receive(parent_data, dst, msg);
	}
}

int do_transfers(int*** matrix, local_id proc_numb, int N, int log_fd, balance_t* balance, BalanceHistory* balanceHistory){
	Message* msg=(Message*)malloc(sizeof(Message));
	SelfStruct* selfStruct = (SelfStruct*)malloc(sizeof(SelfStruct));
	local_id from, to;
	MessageType msg_type;
	balance_t amount;
	TransferOrder* transferOrder = (TransferOrder*)malloc(sizeof(TransferOrder));
	BalanceState* balanceState = (BalanceState*)malloc(sizeof(BalanceState));
	timestamp_t time;

	balanceState->s_balance_pending_in = 0;

	selfStruct->N = N;
	selfStruct->fd_matrix = matrix;
	selfStruct->src = proc_numb;
	selfStruct->log_fd = log_fd;

	while(1){
		if(receive(selfStruct, PARENT_ID, msg)==-1){
			return -1;
		}
		transferOrder = (TransferOrder*)(msg->s_payload);
		from = transferOrder -> s_src;
		to = transferOrder -> s_dst;
		msg_type = msg->s_header.s_type;

		if(msg_type == TRANSFER){
			if(proc_numb == from){
				amount = transferOrder -> s_amount;
				(*balance) -= amount;

				balanceState -> s_balance = *balance;
				time = get_physical_time();
				balanceState -> s_time = time;
				balanceHistory->s_history[time] = *balanceState;

				transfer(selfStruct, from, to, amount);
			}else if(proc_numb == to){
				receive(selfStruct, from, msg);
				transferOrder = (TransferOrder*)(msg->s_payload);
				amount = transferOrder -> s_amount;
				(*balance) += amount;

				balanceState -> s_balance = *balance;
				time = get_physical_time();
				balanceState -> s_time = time;
				balanceHistory->s_history[time] = *balanceState;

				send(selfStruct, PARENT_ID, create_message(ACK, NULL, 0));
			}
		}else if(msg_type == STOP){
			break;
		}
	}
	balanceHistory->s_id = proc_numb;
	balanceHistory -> s_history_len = time + 1;
	complete_history(balanceHistory);
	return 0;
}

void complete_history(BalanceHistory* balanceHistory){
	int i=0;
	BalanceState* prev;

	for(i = 1; i <= balanceHistory->s_history_len/2; i++){
		balanceHistory->s_history[i] = balanceHistory->s_history[2*i];
		balanceHistory->s_history[i].s_time = i;
	}

	 balanceHistory -> s_history_len = balanceHistory -> s_history_len/2 +1;

	for(i = 1; i <= balanceHistory->s_history_len; i++){
		prev = &balanceHistory->s_history[i-1];
		if(balanceHistory->s_history[i].s_balance == 0){
			BalanceState* bs=(BalanceState*)malloc(sizeof(BalanceState));
			bs->s_balance=prev->s_balance;
			bs->s_balance_pending_in=0;
			bs->s_time=i;
			balanceHistory->s_history[i] = *bs;
		}
	}

}

int send_history(int*** matrix, local_id proc_numb, int N, int log_fd, BalanceHistory* balanceHistory){
	SelfStruct* selfStruct = create_self_struct(matrix, proc_numb, N, log_fd);
	Message* msg = create_message(BALANCE_HISTORY, (char*)balanceHistory, (balanceHistory->s_history_len)*sizeof(BalanceState)+sizeof(uint8_t)+sizeof(local_id));

	send(selfStruct, PARENT_ID, msg);
	return 0;
}

AllHistory* receive_and_print_all_history(int*** matrix, local_id proc_numb, int N, int log_fd){
	int i;
	Message* msg=(Message*)malloc(sizeof(Message));
	AllHistory* allHistory = (AllHistory*)malloc(sizeof(AllHistory));
	allHistory->s_history_len = N;

	for(i = 1; i <= N; i++){
		receive(create_self_struct(matrix, PARENT_ID, N, log_fd), i, msg);
		allHistory->s_history[i-1] = *((BalanceHistory*)(msg->s_payload));
	}
	print_history(allHistory);
	return allHistory;
}

// int main(int argc, char * argv[])
// {
//     //bank_robbery(parent_data);
//     //print_history(all);

//     return 0;
// }
