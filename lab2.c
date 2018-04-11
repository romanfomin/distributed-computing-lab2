#include "banking.h"
#include "lab1.h"
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

int do_transfers(int*** matrix, local_id proc_numb, int N, int log_fd, balance_t* balance){
	Message* msg=(Message*)malloc(sizeof(Message));
	SelfStruct* selfStruct = (SelfStruct*)malloc(sizeof(SelfStruct));
	local_id from, to;
	MessageType msg_type;
	balance_t amount;
	TransferOrder* transferOrder = (TransferOrder*)malloc(sizeof(TransferOrder));

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
				transfer(selfStruct, from, to, amount);
			}else if(proc_numb == to){
				receive(selfStruct, from, msg);
				transferOrder = (TransferOrder*)(msg->s_payload);
				amount = transferOrder -> s_amount;
				(*balance) += amount;
				send(selfStruct, PARENT_ID, create_message(ACK,NULL,0));
			}
		}else if(msg_type == STOP){
			break;
		}
	}
}

// int main(int argc, char * argv[])
// {
//     //bank_robbery(parent_data);
//     //print_history(all);

//     return 0;
// }
