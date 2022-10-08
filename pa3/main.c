#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include "log3pa.h"
#include "communication.h"
#include "banking.h"
#include "ltime.h"

/* define return status */
#define ERROR_INVALID_ARGUMENTS -1
#define ERROR_FORK -2
#define SUCCESS 0

int get_proc_count(int argc, char** argv);

int parent_handler(PipesCommunication* comm);
int child_handler(PipesCommunication* comm);

int transfer_message(PipesCommunication* comm, Message* msg, BalanceState* state, BalanceHistory* history);
void update_history(BalanceState* state, BalanceHistory* history, balance_t amount, timestamp_t timestamp_msg, char inc, char fix);

/**
 * @return -1 on invalid arguments, -2 on fork error, 0 on success
 */
int main(int argc, char** argv)
{
	size_t i;
	int child_count;
	int* pipes;
	pid_t* children;
	pid_t fork_id;
	local_id current_proc_id;
	PipesCommunication* comm;
	
	// ������
	if (argc < 4 || (child_count = get_proc_count(argc, argv)) == -1)
	{
		fprintf(stderr, "Usage: %s -p X y1 y2 ... yX\n", argv[0]);
		return ERROR_INVALID_ARGUMENTS;
	}
	
	// ��ʼ����־
	log_init();
	
	// �����ڴ�
	children = malloc(sizeof(pid_t) * child_count);
	
	// Ϊ���н��̴򿪹ܵ�
	pipes = pipes_init(child_count + 1);
	
	// �����ӽ���
	for (i = 0; i < child_count; i++)
	{
		fork_id = fork();
		if (fork_id < 0)
		{
			return ERROR_FORK;
		}
		else if (!fork_id)
		{
			free(children);
			break;
		}
		children[i] = fork_id;
	}
	
	// ���õ�ǰ����ID
	current_proc_id = (fork_id == 0) i + 1 : PARENT_ID;

	
	// Ϊ�������ùܵ�fds  */
	balance_t balance = atoi(argv[proc_id + 2]); //��ó�ʼ���
	comm = communication_init(pipes, child_count + 1, current_proc_id, balance);
	log_pipes(comm);
	
	// ���빤������
	if (current_proc_id == PARENT_ID)
	{
		parent_handler(comm);
		for (i = 0; i < child_count; i++) 
		{ // ����Ǹ����̣��ȴ������ӽ��̽���
			waitpid(children[i], NULL, 0);
		}
	}
	else
	{
		child_handler(comm);
	}
	
	
	// ����
	log_destroy();//�ͷ���־�ļ�
	communication_destroy(comm);//�ͷŹܵ�
	return 0;
}

/** 
 * �����̴�����
 * ���������Ϣ���˵��������ʷ��¼
 * 
 * @param comm		�ܵ�������ָ��
 *
 * @return -1 ����ȷ����Ϣ����, 0 ��������.
 */
int parent_handler(PipesCommunication* comm)
{
	AllHistory all_history; //�����¼
	local_id i;
	
	all_history.s_history_len = comm->total_ids - 1; //�����ӽ�������
	
    receive_all_msgs(comm, STARTED); //�ȴ��������̵ľ�����Ϣ

    /* �����˵� */
    bank_robbery(comm, comm->total_ids - 1);

	/* ������ɣ��ȴ��ӽ��̽��� */
	increment_lamport_time();
    send_all_stop_msg(comm);
    receive_all_msgs(comm, DONE);
	
	/* �����ʷ��¼ */
	for (i = 1; i < comm->total_ids; i++){
		BalanceHistory balance_history;
		Message msg;
		
		while(receive(comm, i, &msg));
		
		if (msg.s_header.s_type != BALANCE_HISTORY){
			return -1;
		}
		
		memcpy((void*)&balance_history, msg.s_payload, sizeof(char) * msg.s_header.s_payload_len);
		all_history.s_history[i - 1] = balance_history;
	}
	
	print_history(&all_history);
	return 0;
}

/** �ӽ��̴�����
 *
 * @param comm		�ܵ�������ָ��
 *
 * @return -1 ����ȷ����Ϣ����, 0 ��������.
 */
int child_handler(PipesCommunication* comm)
{
	BalanceState balance_state; //���״̬
    BalanceHistory balance_history; //�����ʷ
	size_t done_left = comm->total_ids - 2;
	int not_stopped = 1;

    balance_history.s_id = comm->current_id;
	
	balance_state.s_balance = comm->balance;
    balance_state.s_balance_pending_in = 0;
    balance_state.s_time = 0;
	
	update_history(&balance_state, &balance_history, 0, 0, 0, 0);
	
	// ���Ͳ����ܾ�����Ϣ
	increment_lamport_time();
	send_all_proc_event_msg(comm, STARTED);
	increment_lamport_time();
    receive_all_msgs(comm, STARTED);
	
	// ����ת�ˣ�ֹͣ�������Ϣ
	while(done_left || not_stopped){
		Message msg;
		
        while (receive_any(comm, &msg));
		
		if (msg.s_header.s_type == TRANSFER){
			transfer_message(comm, &msg, &balance_state, &balance_history);
		}
		else if (msg.s_header.s_type == STOP){
			update_history(&balance_state, &balance_history, 0, msg.s_header.s_local_time, 1, 0);
			send_all_proc_event_msg(comm, DONE);
			not_stopped = 0;
		}
		else if (msg.s_header.s_type == DONE){
			update_history(&balance_state, &balance_history, 0, msg.s_header.s_local_time, 1, 0);
			done_left--;
		}
		else{
			return -1;
		}
	}
	
	log_received_all_done(comm->current_id); //�����������̵������Ϣ
	
	// ������ʷ��¼�����͸�������
	update_history(&balance_state, &balance_history, 0, 0, 1, 0);
	send_balance_history(comm, PARENT_ID, &balance_history);
	return 0;
}

/** 
 * ����ת����Ϣ
 * @param comm		�ܵ�������ָ��
 * @param msg		ת����Ϣ
 * @param state		���״̬
 * @param history	�����ʷ��¼
 *
 * @return -1 �Ƿ���ַ, -2 ������Ϣ����, 0 �ɹ�.
 */
int transfer_message(PipesCommunication* comm, Message* msg, BalanceState* state, BalanceHistory* history)
{
	TransferOrder order;
	memcpy(&order, msg->s_payload, sizeof(char) * msg->s_header.s_payload_len);
	
	update_history(state, history, 0, 0, 1, 0);
	
	// ����֧��Transfer request */
	if (comm->current_id == order.s_src)
	{
		update_history(state, history, -order.s_amount, msg->s_header.s_local_time, 1, 0);
		update_history(state, history, 0, 0, 1, 0);
		send_transfer_msg(comm, order.s_dst, &order);
		comm->balance -= order.s_amount;
	}
	/* ��������Transfer income */
	else if (comm->current_id == order.s_dst)
	{
		update_history(state, history, order.s_amount, msg->s_header.s_local_time, 1, 1);
		increment_lamport_time();
		send_ack_msg(comm, PARENT_ID);
		comm->balance += order.s_amount;
	}
	else
	{
		return -1;
	}
	return 0;
}

/** ���·��������ʷ��¼
 *
 * @param state				���״̬
 * @param history			�����ʷ��¼
 * @param amount			���䶯���
 * @param timestamp_msg		��Ϣʱ���
 * @param inc				����ʱ����
 * @param fix				Fix flag
 */
void update_history(BalanceState* state, BalanceHistory* history, balance_t amount, timestamp_t timestamp_msg, char inc, char fix){
	static timestamp_t prev_time = 0;
    timestamp_t curr_time = get_lamport_time() < timestamp_msg ? timestamp_msg : get_lamport_time();
	timestamp_t i;
	
	if (inc){
		curr_time++;
	}
	set_lamport_time(curr_time);
	if (fix){
		timestamp_msg--;
	}

    history->s_history_len = curr_time + 1;
	
	for (i = prev_time; i < curr_time; i++){
		state->s_time = i;
		history->s_history[i] = *state;
	}
	
	if (amount > 0){
		for (i = timestamp_msg; i < curr_time; i++){
			history->s_history[i].s_balance_pending_in += amount;
		}
	}
	
	prev_time = curr_time;
	state->s_time = curr_time;
	state->s_balance += amount;
	history->s_history[curr_time] = *state;
}

/** �������в����еõ��ӽ�����
 *
 * @param argc		��������
 * @param argv		�����ַ�������ָ��
 *
 * @return -1 on error, any other values on success.
 */
int get_proc_count(int argc, char** argv){
	int proc_count;
	if (!strcmp(argv[1], "-p") && (proc_count = atoi(argv[2])) == (argc - 3)){
		return proc_count;
	}
	return -1;
}
