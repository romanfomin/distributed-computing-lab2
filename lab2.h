#ifndef _LAB2_H
#define _LAB2_H

#include "banking.h"

int do_transfers(int*** matrix, local_id proc_numb, int N, int log_fd, balance_t* balance, BalanceHistory* balanceHistory);
int send_history(int*** matrix, local_id proc_numb, int N, int log_fd, BalanceHistory* balanceHistory);
AllHistory* receive_and_print_all_history(int*** matrix, local_id proc_numb, int N, int log_fd);
void complete_history(BalanceHistory* balanceHistory);

#endif
