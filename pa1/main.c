/**
 * @file     main.c
 * @Author   @seniorkot
 * @date     May, 2018
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

#include "pa1.h"
#include "ipc.h"
#include "log1pa.h"
#include "communication.h"

#define BUFFER_SIZE 512

int get_proc_count(char** argv);
int send_msg(PipesCommunication* comm, MessageType type); //发送消息
int recieve_msgs(PipesCommunication* comm, MessageType type); //接收消息

/**
 * @return  -1 无效参数, -2 fork失败, 0成功
 * 
 */
int main(int argc, char** argv){
	size_t i;
	int proc_count;
	local_id current_proc_id;
	pid_t fork_id;
	pid_t* children;
	int* pipes;
	PipesCommunication* comm;
	
	/* 解析程序参数 */
	switch(argc){
		case 1:
			proc_count = 1; /* TODO: 设置默认程序 */
			break;
		case 3:
			proc_count = get_proc_count(argv);
			if (proc_count <= 0 || proc_count > MAX_PROCESS_ID + 1){
				fprintf(stderr, "Usage: %s -p (1-16)\n", argv[0]);
				return -1;
			}
			break;
		default:
			fprintf(stderr, "Usage: %s -p (1-16)\n", argv[0]);
			return -1;
	}
	
	/* 初始化日志 */
	log_init();
	
	/* 为子进程分配内存 */
	children = malloc(sizeof(pid_t) * proc_count);
	
	/* 为所有进程打开管道 */
	pipes = pipes_init(proc_count + 1);
	
	/* 创建子进程 */
	for (i = 0; i < proc_count; i++){
		fork_id = fork();
		if (fork_id < 0){
			return -2;
		}
		else if (fork_id == 0){ //代表子进程
			free(children);
			break;
		}
		children[i] = fork_id; //记录id
	}
	
	/* 设置当前id */
	if (fork_id == 0){
		current_proc_id = i + 1;
	}
	else{
		current_proc_id = PARENT_ID;
	}
	
	/* Set pipe fds to process params */
	comm = communication_init(pipes, proc_count + 1, current_proc_id);
	log_pipes(comm);
	
	/* 发送和接收开始消息 */
	if (current_proc_id != PARENT_ID){
		send_msg(comm, STARTED);
	}
	recieve_msgs(comm, STARTED);
	
	/* 发送和接收结束消息 */
	if (current_proc_id != PARENT_ID){
		send_msg(comm, DONE);
	}
	recieve_msgs(comm, DONE);
	
	/* 如果父进程等待所有子进程 */
	if (current_proc_id == PARENT_ID){
		for (i = 0; i < proc_count; i++){
			waitpid(children[i], NULL, 0);
		}
	}
	
	log_destroy(); //记录删除消息
	communication_destroy(comm);
	return 0;
}

/** 从命令行参数得到进程数量
 *
 * @param argv		包含命令行参数的双字符数组
 *
 * @return -1 错误, 其他值表示后面的参数.
 */
int get_proc_count(char** argv){
	if (!strcmp(argv[1], "-p")){
		return atoi(argv[2]);
	}
	return -1;
}

/** 发送消息给其他所有进程
 *
 * @param comm		通信管道指针
 * @param type		消息类型: STARTED / DONE
 *
 * @return -1 on 不正确的类型, -2 on 内部错误, -3 on 发送错误, 0 on 成功.
 */
int send_msg(PipesCommunication* comm, MessageType type){
	Message msg; //消息体
	uint16_t length = 0; //长度
	char buf[BUFFER_SIZE];
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = type;
    msg.s_header.s_local_time = time(NULL);
	
	type == STARTED ? log_started(comm->current_id) : log_done(comm->current_id);
	
	switch (type){
        case STARTED:
			length = snprintf(buf, BUFFER_SIZE, log_started_fmt, comm->current_id, getpid(), getppid());
			break;
		case DONE:
			length = snprintf(buf, BUFFER_SIZE, log_done_fmt, comm->current_id);
			break;
		default:
			return -1;
	}
	
	if (length <= 0){
		return -2;
	}
	
	msg.s_header.s_payload_len = length;
    memcpy(msg.s_payload, buf, sizeof(char) * length);
	
	if(send_multicast(comm, &msg)){
		return -3;
	}
	
	return 0;
}

/** 从其他进程接收消息
 *
 * @param comm		通信管道指针
 * @param type		消息类型: STARTED / DONE
 *
 * @return -1 接收消息错误, 0 成功.
 */
int recieve_msgs(PipesCommunication* comm, MessageType type){
	Message msg;
	
	if (receive_any(comm, &msg)){
		return -1;
	}
	
	switch (type){
        case STARTED:
            log_received_all_started(comm->current_id);
            break;
        case DONE:
            log_received_all_done(comm->current_id);
            break;
		default:
			break;
    }
	return 0;
}
