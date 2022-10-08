#include "log3pa.h"
#include "common.h"
#include "pa2345.h"

#include <stdio.h>
#include <unistd.h>

FILE* pipes_log_f;
FILE* events_log_f;

/**
* 初始化日志
*/
void log_init(){
	pipes_log_f = fopen(pipes_log, "w");
	events_log_f = fopen(events_log, "w");
}

/**
* 释放日志
*/
void log_destroy(){
	fclose(pipes_log_f);
    fclose(events_log_f);
}

/**
* 记录管道创建
*/
void log_pipes(PipesCommunication* comm){
	size_t i;
	
	fprintf(pipes_log_f, "Process %d pipes:\n", comm->current_id);
	
	for (i = 0; i < comm->total_ids; i++){
		if (i == comm->current_id){
			continue;
		}
		
		fprintf(pipes_log_f, "P%ld|R%d|W%d ", i, comm->pipes[(i < comm->current_id ? i : i-1) * 2 + PIPE_READ_TYPE], 
			comm->pipes[(i < comm->current_id ? i : i-1) * 2 + PIPE_WRITE_TYPE]);
	}
	fprintf(pipes_log_f, "\n");
}

/**
* 记录初始余额
*/
void log_started(local_id id, balance_t balance){
	printf(log_started_fmt, get_lamport_time(), id, getpid(), getppid(), balance);
    fprintf(events_log_f, log_started_fmt, get_lamport_time(), id, getpid(), getppid(), balance);
}


/**
* 记录接收到所有进程就绪消息
*/
void log_received_all_started(local_id id){
	printf(log_received_all_started_fmt, get_lamport_time(), id);
    fprintf(events_log_f, log_received_all_started_fmt, get_lamport_time(), id);
}

/**
* 记录子进程完成
*/
void log_done(local_id id, balance_t balance){
	printf(log_done_fmt, get_lamport_time(), id, balance);
    fprintf(events_log_f, log_done_fmt, get_lamport_time(), id, balance);
}

/**
* 记录接受所有进程完成
*/
void log_received_all_done(local_id id){
	printf(log_received_all_done_fmt, get_lamport_time(), id);
    fprintf(events_log_f, log_received_all_done_fmt, get_lamport_time(), id);
}

/**
* 记录转出
*/
void log_transfer_out(local_id from, local_id dst, balance_t amount){
	printf(log_transfer_out_fmt, get_lamport_time(), from, amount, dst);
	fprintf(events_log_f, log_transfer_out_fmt, get_lamport_time(), from, amount, dst);
}

/**
* 记录转入
*/
void log_transfer_in(local_id from, local_id dst, balance_t amount){
	printf(log_transfer_in_fmt, get_lamport_time(), dst, amount, from);
	fprintf(events_log_f, log_transfer_in_fmt, get_lamport_time(), dst, amount, from);
}
