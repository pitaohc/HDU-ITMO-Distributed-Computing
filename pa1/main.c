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
int send_msg(PipesCommunication* comm, MessageType type); //������Ϣ
int recieve_msgs(PipesCommunication* comm, MessageType type); //������Ϣ

/**
 * @return  -1 ��Ч����, -2 forkʧ��, 0�ɹ�
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
	
	/* ����������� */
	switch(argc){
		case 1:
			proc_count = 1; /* TODO: ����Ĭ�ϳ��� */
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
	
	/* ��ʼ����־ */
	log_init();
	
	/* Ϊ�ӽ��̷����ڴ� */
	children = malloc(sizeof(pid_t) * proc_count);
	
	/* Ϊ���н��̴򿪹ܵ� */
	pipes = pipes_init(proc_count + 1);
	
	/* �����ӽ��� */
	for (i = 0; i < proc_count; i++){
		fork_id = fork();
		if (fork_id < 0){
			return -2;
		}
		else if (fork_id == 0){ //�����ӽ���
			free(children);
			break;
		}
		children[i] = fork_id; //��¼id
	}
	
	/* ���õ�ǰid */
	if (fork_id == 0){
		current_proc_id = i + 1;
	}
	else{
		current_proc_id = PARENT_ID;
	}
	
	/* Set pipe fds to process params */
	comm = communication_init(pipes, proc_count + 1, current_proc_id);
	log_pipes(comm);
	
	/* ���ͺͽ��տ�ʼ��Ϣ */
	if (current_proc_id != PARENT_ID){
		send_msg(comm, STARTED);
	}
	recieve_msgs(comm, STARTED);
	
	/* ���ͺͽ��ս�����Ϣ */
	if (current_proc_id != PARENT_ID){
		send_msg(comm, DONE);
	}
	recieve_msgs(comm, DONE);
	
	/* ��������̵ȴ������ӽ��� */
	if (current_proc_id == PARENT_ID){
		for (i = 0; i < proc_count; i++){
			waitpid(children[i], NULL, 0);
		}
	}
	
	log_destroy(); //��¼ɾ����Ϣ
	communication_destroy(comm);
	return 0;
}

/** �������в����õ���������
 *
 * @param argv		���������в�����˫�ַ�����
 *
 * @return -1 ����, ����ֵ��ʾ����Ĳ���.
 */
int get_proc_count(char** argv){
	if (!strcmp(argv[1], "-p")){
		return atoi(argv[2]);
	}
	return -1;
}

/** ������Ϣ���������н���
 *
 * @param comm		ͨ�Źܵ�ָ��
 * @param type		��Ϣ����: STARTED / DONE
 *
 * @return -1 on ����ȷ������, -2 on �ڲ�����, -3 on ���ʹ���, 0 on �ɹ�.
 */
int send_msg(PipesCommunication* comm, MessageType type){
	Message msg; //��Ϣ��
	uint16_t length = 0; //����
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

/** ���������̽�����Ϣ
 *
 * @param comm		ͨ�Źܵ�ָ��
 * @param type		��Ϣ����: STARTED / DONE
 *
 * @return -1 ������Ϣ����, 0 �ɹ�.
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
