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

/* 定义主函数返回类型 */
#define ERROR_INVALID_ARGUMENTS -1
#define ERROR_FORK -2
#define SUCCESS 0

#define true 1
#define false 0

int get_proc_count(int argc, char** argv);

int parent_handler(PipesCommunication* comm);
int child_handler(PipesCommunication* comm);

int transfer_message(PipesCommunication* comm, Message* msg, BalanceState* state, BalanceHistory* history);
void update_history(BalanceState* state, BalanceHistory* history, balance_t amount, timestamp_t timestamp_msg, char inc, char fix);

/**
 * @return -1 无效参数, -2 创建子进程错误, 0 正常结束
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
	
	// 检查参数
	if (argc < 4 || (child_count = get_proc_count(argc, argv)) == -1)
	{
		//fprintf(stderr, "Usage: %s -p X y1 y2 ... yX\n", argv[0]);
		return ERROR_INVALID_ARGUMENTS;
	}
	
	// 初始化日志
	log_init();
	
	// 分配内存
	children = malloc(sizeof(pid_t) * child_count);
	
	// 为所有进程打开管道
	pipes = pipes_init(child_count + 1);
	
	// 创建子进程
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
	
	// 设置当前进程ID
	current_proc_id = (fork_id == 0) i + 1 : PARENT_ID;

	
	// 为进程设置管道fds  */
	balance_t balance = atoi(argv[proc_id + 2]); //获得初始金额
	comm = communication_init(pipes, child_count + 1, current_proc_id, balance);
	log_pipes(comm);
	
	// 进入工作函数
	if (current_proc_id == PARENT_ID)
	{
		parent_handler(comm);
		for (i = 0; i < child_count; i++) 
		{ // 如果是父进程，等待所有子进程结束
			waitpid(children[i], NULL, 0);
		}
	}
	else
	{
		child_handler(comm);
	}
	
	
	// 后处理
	log_destroy();//释放日志文件
	communication_destroy(comm);//释放管道
	return 0;
}

/** 
 * 父进程处理函数
 * 负责接收消息，账单，输出历史记录
 * 
 * @param comm		管道管理器指针
 *
 * @return -1 不正确的消息类型, 0 正常返回.
 */
int parent_handler(PipesCommunication* comm)
{
	AllHistory all_history; //总体记录
	
	all_history.s_history_len = comm->total_ids - 1; //等于子进程数量
	
    receive_all_msgs(comm, STARTED); //等待其他进程的就绪消息

    /* 处理账单 */
    bank_robbery(comm, comm->total_ids - 1);

	/* 处理完成，等待子进程结束 */
	increment_lamport_time();
    send_all_stop_msg(comm);
    receive_all_msgs(comm, DONE);
	
	/* 输出历史记录 */
	for (local_id i = 1; i < comm->total_ids; i++)
	{
		BalanceHistory balance_history;
		Message msg;
		
		while(receive(comm, i, &msg));
		
		if (msg.s_header.s_type != BALANCE_HISTORY)
		{
			return -1;
		}
		
		memcpy((void*)&balance_history, msg.s_payload, sizeof(char) * msg.s_header.s_payload_len);
		all_history.s_history[i - 1] = balance_history;
	}
	
	print_history(&all_history);
	return 0;
}

/** 子进程处理函数
 *
 * @param comm		管道管理器指针
 *
 * @return -1 不正确的消息类型, 0 正常返回.
 */
int child_handler(PipesCommunication* comm)
{
	BalanceState balance_state; //余额状态
    BalanceHistory balance_history; //余额历史
	size_t done_left = comm->total_ids - 2;
	int stopped = false;

    balance_history.s_id = comm->current_id;
	
	balance_state.s_balance = comm->balance;
    balance_state.s_balance_pending_in = 0;
    balance_state.s_time = 0;
	
	update_history(&balance_state, &balance_history, 0, 0, 0, 0);
	
	// 发送并接受就绪消息
	increment_lamport_time();
	send_all_proc_event_msg(comm, STARTED);
	increment_lamport_time();
    receive_all_msgs(comm, STARTED);
	
	// 发送转账，停止，完成消息
	while(done_left || !not_stopped)
	{
		Message msg;
		
        while (receive_any(comm, &msg));
		
		if (msg.s_header.s_type == TRANSFER)
		{
			transfer_message(comm, &msg, &balance_state, &balance_history);
		}
		else if (msg.s_header.s_type == STOP)
		{
			update_history(&balance_state, &balance_history, 0, msg.s_header.s_local_time, 1, 0);
			send_all_proc_event_msg(comm, DONE);
			stopped = true;
		}
		else if (msg.s_header.s_type == DONE)
		{
			update_history(&balance_state, &balance_history, 0, msg.s_header.s_local_time, 1, 0);
			done_left--;
		}
		else
		{
			return -1;
		}
	}
	
	log_received_all_done(comm->current_id); //接受其他进程的完成消息
	
	// 更新历史记录并发送给父进程
	update_history(&balance_state, &balance_history, 0, 0, 1, 0);
	send_balance_history(comm, PARENT_ID, &balance_history);
	return 0;
}

/** 
 * 处理转账消息
 * @param comm		管道管理器指针
 * @param msg		转账消息
 * @param state		余额状态
 * @param history	余额历史记录
 *
 * @return -1 非法地址, -2 发送消息错误, 0 成功.
 */
int transfer_message(PipesCommunication* comm, Message* msg, BalanceState* state, BalanceHistory* history)
{
	TransferOrder order;
	memcpy(&order, msg->s_payload, sizeof(char) * msg->s_header.s_payload_len);
	
	update_history(state, history, 0, 0, 1, 0);
	
	// 处理支出Transfer request */
	if (comm->current_id == order.s_src)
	{
		update_history(state, history, -order.s_amount, msg->s_header.s_local_time, 1, 0);
		update_history(state, history, 0, 0, 1, 0);
		send_transfer_msg(comm, order.s_dst, &order);
		comm->balance -= order.s_amount;
	}
	/* 处理收入Transfer income */
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

/** 更新分行余额历史记录
 *
 * @param state				余额状态
 * @param history			余额历史记录
 * @param amount			余额变动金额
 * @param timestamp_msg		消息时间戳
 * @param inc				增加时间标记
 * @param fix				Fix flag
 */
void update_history(BalanceState* state, BalanceHistory* history, balance_t amount, timestamp_t timestamp_msg, char inc, char fix)
{
	static timestamp_t prev_time = 0;
    timestamp_t curr_time = get_lamport_time() < timestamp_msg ? timestamp_msg : get_lamport_time();
	timestamp_t i;
	
	if (inc)
	{
		curr_time++;
	}
	set_lamport_time(curr_time);
	if (fix)
	{
		timestamp_msg--;
	}

    history->s_history_len = curr_time + 1;
	
	for (i = prev_time; i < curr_time; i++)
	{
		state->s_time = i;
		history->s_history[i] = *state;
	}
	
	if (amount > 0)
	{
		for (i = timestamp_msg; i < curr_time; i++)
		{
			history->s_history[i].s_balance_pending_in += amount;
		}
	}
	
	prev_time = curr_time;
	state->s_time = curr_time;
	state->s_balance += amount;
	history->s_history[curr_time] = *state;
}

/** 从命令行参数中得到子进程数
 *
 * @param argc		参数数量
 * @param argv		参数字符串数组指针
 *
 * @return -1 on error, any other values on success.
 */
int get_proc_count(int argc, char** argv)
{
	int proc_count;
	if (!strcmp(argv[1], "-p") && (proc_count = atoi(argv[2])) == (argc - 3))
	{
		return proc_count;
	}
	return -1;
}
