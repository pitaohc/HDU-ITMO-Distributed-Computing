#ifndef __IFMO_DISTRIBUTED_CLASS_COMMUNICATION__H
#define __IFMO_DISTRIBUTED_CLASS_COMMUNICATION__H

#include "ipm.h"
#include "banking.h"

typedef struct
{
    int* pipes;
    local_id current_id;
    size_t total_ids;
    balance_t balance;
} PipeManager;

enum PipeTypeOffset
{
    PIPE_READ_TYPE = 0,
    PIPE_WRITE_TYPE = 1,
};

enum RESULT_SET_NONBLOCK
{
    ERROR_SET_NONBLOCK_NO_SET = -2,
    ERROR_SET_NONBLOCK_NO_FLAGS = -1,
    RESULT_SET_NONBLOCK_SUCCESS = 0,
};

int* pipes_init(size_t proc_count);
PipeManager* communication_init(int* pipes, size_t proc_count, local_id curr_proc, balance_t balance);
void communication_release(PipeManager* pm);

int send_all_proc_event_msg(PipeManager* pm, MessageType type);
void send_all_stop_msg(PipeManager* pm);
void send_transfer_msg(PipeManager* pm, local_id dst, TransferOrder* order);
void send_ack_msg(PipeManager* pm, local_id dst);
void send_balance_history(PipeManager* pm, local_id dst, BalanceHistory* history);

void receive_all_msgs(PipeManager* pm, MessageType type);

#endif
