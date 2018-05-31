/**
 * @file     communication.c
 * @Author   @seniorkot
 * @date     May, 2018
 * @brief    Functions that help to organize IPC
 */
 
#include "communication.h"

#define BUFFER_SIZE 512

/** Set 0_NONBLOCK flag to fd
 * 
 * @param fd			File Descriptor
 *
 * @return -1 if can't get flags, -2 if can't set flags, 0 on success
 */
int set_nonblock(int pipe_id){
	int flags = fcntl(pipe_id, F_GETFL);
    if (flags == -1){
        return -1;
    }
    flags = fcntl(pipe_id, F_SETFL, flags | O_NONBLOCK);
    if (flags == -1){
        return -2;
    }
    return 0;
}

/** Open pipes fds
 * 
 * @param proc_count    Process count including parent process.
 *
 * @return pointer to pipe fds array
 */
int* pipes_init(size_t proc_count){
	size_t i, j;
	size_t offset = proc_count - 1;
	int* pipes = malloc(sizeof(int) * 2 * proc_count * (proc_count-1));
	
	for (i = 0; i < proc_count; i++){
		for (j = 0; j < proc_count; j++){
			int tmp_fd[2];
			
			if (i == j){
				continue;
			}
			
			if (pipe(tmp_fd) < 0){
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

/** Init PipesCommunication
 * 
 * @param pipes			Pointer to opened pipes fd
 * @param proc_count    Process count including parent process.
 * @param curr_proc		Current process local id
 *
 * @return pointer to PipesCommunication
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
	
	/* Close unnecessary fds */
	for (i = 0; i < proc_count; i++){
		if (i == curr_proc){
			continue;
		}
		for (j = 0; j < proc_count; j++){
			close(pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_READ_TYPE]);
			close(pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_WRITE_TYPE]);
		}
	}
	free(pipes);
	return this;
}

/** Close all fds & free space
 * 
 * @param comm		Pointer to PipesCommunication
 */
void communication_destroy(PipesCommunication* comm){
	size_t i;
	for (i = 0; i < comm->total_ids - 1; i++){
		close(comm->pipes[i * 2 + PIPE_READ_TYPE]);
		close(comm->pipes[i * 2 + PIPE_WRITE_TYPE]);
	}
	free(comm);
}

/** Send event (STARTED / DONE) message to all processes
 * 
 * @param comm		Pointer to PipesCommunication
 * @param type		Message type: STARTED / DONE
 *
 * @return -1 on incorrect type, -2 on internal error, -3 on sending message error, 0 on success
 */
int send_all_proc_event_msg(PipesCommunication* comm, MessageType type){
	Message* msg = malloc(sizeof(Message));
	uint16_t length = 0;
	char buf[BUFFER_SIZE];
	
	msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_type = type;
    msg->s_header.s_local_time = get_physical_time();
	
	type == STARTED ? log_started(comm->current_id, comm->balance) : log_done(comm->current_id, comm->balance);
	
	switch (type){
        case STARTED:
			length = snprintf(buf, BUFFER_SIZE, log_started_fmt, get_physical_time(), comm->current_id, getpid(), getppid(), comm->balance);
			break;
		case DONE:
			length = snprintf(buf, BUFFER_SIZE, log_done_fmt, get_physical_time(), comm->current_id, comm->balance);
			break;
		default:
			free(msg);
			return -1;
	}
		
	if (length <= 0){
		free(msg);
		return -2;
	}
	
	msg->s_header.s_payload_len = length;
    memcpy(msg->s_payload, buf, sizeof(char) * length);
	
	if(send_multicast(comm, msg)){
		free(msg);
		return -3;
	}
	
	free(msg);
	return 0;
}

/** Send STOP message to all processes
 * 
 * @param comm		Pointer to PipesCommunication
 *
 * @return -1 on sending message error, 0 on success
 */
int send_all_stop_msg(PipesCommunication* comm){
	Message* msg = malloc(sizeof(Message));
	msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_type = STOP;
    msg->s_header.s_local_time = get_physical_time();
	msg->s_header.s_payload_len = 0;
	
	if(send_multicast(comm, msg)){
		free(msg);
		return -1;
	}
	free(msg);
	return 0;
}

/** Send TRANSFER message to all processes
 * 
 * @param comm		Pointer to PipesCommunication
 *
 * @return -1 on sending message error, 0 on success
 */
int send_transfer_msg(PipesCommunication* comm, local_id dst, TransferOrder* order){
	Message* msg = malloc(sizeof(Message));
	msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_type = TRANSFER;
    msg->s_header.s_local_time = get_physical_time();
	msg->s_header.s_payload_len = sizeof(TransferOrder);
	
	memcpy(msg->s_payload, order, msg->s_header.s_payload_len);
	
	if (send(comm, dst, msg)){
		free(msg);
		return -1;
	}
	free(msg);
	return 0;
}

/** Send ACK message to all processes
 * 
 * @param comm		Pointer to PipesCommunication
 *
 * @return -1 on sending message error, 0 on success
 */
int send_ack_msg(PipesCommunication* comm, local_id dst){
	Message* msg = malloc(sizeof(Message));
	msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_type = ACK;
    msg->s_header.s_local_time = get_physical_time();
	msg->s_header.s_payload_len = 0;
	
	if (send(comm, dst, msg)){
		free(msg);
		return -1;
	}
	free(msg);
	return 0;
}