#include "communication.h"
#include "log3pa.h"
#include "pa2345.h"
#include "ltime.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/** Set 0_NONBLOCK flag to fd
 * 
 * @param fd			�ļ�������
 *
 * @return -1 if can't get flags, -2 if can't set flags, 0 on success
 */
int set_nonblock(int pipe_id)
{
	int flags = fcntl(pipe_id, F_GETFL);
    if (flags == -1)
	{
        return -1;
    }
    flags = fcntl(pipe_id, F_SETFL, flags | O_NONBLOCK);
    if (flags == -1)
	{
        return -2;
    }
    return 0;
}

/** �򿪹ܵ��ļ�������
 * 
 * @param proc_count    ���������̵Ľ�������
 *
 * @return �ܵ��ļ�����������ָ��
 */
int* pipes_init(size_t proc_count)
{
	size_t i, j;
	size_t offset = proc_count - 1;
	int* pipes = malloc(sizeof(int) * 2 * proc_count * (proc_count-1));
	
	for (i = 0; i < proc_count; i++)
	{
		for (j = 0; j < proc_count; j++)
		{
			int tmp_fd[2];
			
			if (i == j)
			{
				continue;
			}
			
			if (pipe(tmp_fd) < 0)
			{
				return (int*)NULL;
			}
			
			if (set_nonblock(tmp_fd[0]) || set_nonblock(tmp_fd[1])){
				return (int*)NULL;
			}
			pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_READ_TYPE] = tmp_fd[0];  /* READ */
			pipes[j * offset * 2 + (j > i ? i : i - 1) * 2 + PIPE_WRITE_TYPE] = tmp_fd[1]; /* WRITE */
		}
	}
	return pipes;
}

/** ��ʼ���ܵ�ͨѶ
 * 
 * @param pipes			�ܵ��ļ�����������ָ��
 * @param proc_count    ���������̵Ľ�������
 * @param curr_proc		��ǰ���̱���ID
 *
 * @return �ܵ�ͨѶ����ָ��
 */
PipesCommunication* communication_init(int* pipes, size_t proc_count, local_id curr_proc, balance_t balance){
	PipesCommunication* this = malloc(sizeof(PipesCommunication));;
	size_t i, j;
	size_t offset = proc_count - 1;
	this->pipes = malloc(sizeof(int) * offset * 2);
	this->total_ids = proc_count;
	this->current_id = curr_proc;
	this->balance = balance;
	
	memcpy(this->pipes, pipes + curr_proc * 2 * offset, sizeof(int) * offset * 2);
	
	/* �ر����õ��ļ������� */
	for (i = 0; i < proc_count; i++)
	{
		if (i == curr_proc)
		{
			continue;
		}
		for (j = 0; j < proc_count; j++)
		{
			close(pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_READ_TYPE]);
			close(pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_WRITE_TYPE]);
		}
	}
	free(pipes);
	return this;
}

/** �ر������ļ����������ͷ���Դ
 * 
 * @param comm		�ܵ�ͨѶ����ָ��
 */
void communication_destroy(PipesCommunication* comm)
{
	size_t i;
	for (i = 0; i < comm->total_ids - 1; i++)
	{
		close(comm->pipes[i * 2 + PIPE_READ_TYPE]);
		close(comm->pipes[i * 2 + PIPE_WRITE_TYPE]);
	}
	free(comm);
}

/** �����¼���Ϣ�����н���
 * 
 * @param comm		�ܵ�ͨѶ����ָ��
 * @param type		��Ϣ����: STARTED / DONE
 *
 * @return -1 �Ƿ���Ϣ����, -2 �ڲ�����, -3 ������Ϣ����, 0 �ɹ�
 */
int send_all_proc_event_msg(PipesCommunication* comm, MessageType type){
	Message msg;
	uint16_t length = 0;
	char buf[MAX_PAYLOAD_LEN];
	
	msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = type;
    msg.s_header.s_local_time = get_lamport_time();
	
	switch (type)
	{
        case STARTED:
			length = snprintf(buf, MAX_PAYLOAD_LEN, log_started_fmt, get_lamport_time(), comm->current_id, getpid(), getppid(), comm->balance);
			break;
		case DONE:
			length = snprintf(buf, MAX_PAYLOAD_LEN, log_done_fmt, get_lamport_time(), comm->current_id, comm->balance);
			break;
		default:
			return -1;
	}
		
	if (length <= 0)
	{
		return -2;
	}
	
	msg.s_header.s_payload_len = length;
    memcpy(msg.s_payload, buf, sizeof(char) * length);
	
	send_multicast(comm, &msg);
	
	type == STARTED ? log_started(comm->current_id, comm->balance) : log_done(comm->current_id, comm->balance);
	
	return 0;
}

/** ����ֹͣ��Ϣ�����н���
 * 
 * @param comm		�ܵ�ͨѶ����ָ��
 */
void send_all_stop_msg(PipesCommunication* comm)
{
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = STOP;
    msg.s_header.s_local_time = get_lamport_time();
	msg.s_header.s_payload_len = 0;
	
	send_multicast(comm, &msg);
}

/** ����ת����Ϣ
 * 
 * @param comm		�ܵ�ͨѶ����ָ��
 * @param dst 		Ŀ��ID
 * @param order 	�˵���Ϣ
 */
void send_transfer_msg(PipesCommunication* comm, local_id dst, TransferOrder* order){
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = TRANSFER;
    msg.s_header.s_local_time = get_lamport_time();
	msg.s_header.s_payload_len = sizeof(TransferOrder);
	
	memcpy(msg.s_payload, order, msg.s_header.s_payload_len);
	
	while (send(comm, dst, &msg) < 0);
}

/** ����ACK
 * 
 * @param comm		�ܵ�ͨѶ����ָ��
 * @param dst 		Ŀ��ID
 */
void send_ack_msg(PipesCommunication* comm, local_id dst){
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = ACK;
    msg.s_header.s_local_time = get_lamport_time();
	msg.s_header.s_payload_len = 0;
	
	while (send(comm, dst, &msg) < 0);
}

/** ���������ʷ��¼��������
 * 
 * @param comm		�ܵ�ͨѶ����ָ��
 * @param dst 		Ŀ��ID
 * @param history	�����ʷ��Ϣ
 */
void send_balance_history(PipesCommunication* comm, local_id dst, BalanceHistory* history)
{
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = BALANCE_HISTORY;
    msg.s_header.s_local_time = get_lamport_time();
	msg.s_header.s_payload_len = sizeof(BalanceHistory);
	
	memcpy(msg.s_payload, history, msg.s_header.s_payload_len);
	
	while (send(comm, dst, &msg) < 0);
}

/** ����������Ϣ
 * 
 * @param comm		�ܵ�ͨѶ����ָ��
 * @param type		��Ϣ����
 */
void receive_all_msgs(PipesCommunication* comm, MessageType type)
{
	Message msg;
	local_id i;
	
	for (i = 1; i < comm->total_ids; i++)
	{
		if (i == comm->current_id)
		{
			continue;
		}
		while (receive(comm, i, &msg) < 0);
		
		set_lamport_time_from_msg(&msg);
	}
	
	switch (type)
	{
        case STARTED:
            log_received_all_started(comm->current_id);
            break;
        case DONE:
            log_received_all_done(comm->current_id);
            break;
		default:
			break;
    }
}
