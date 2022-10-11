#include "ipc.h"
#include "communication.h"
 
#include <unistd.h>

/**
* 取得序号
*/
int8_t get_index(int8_t x, int8_t id)
{
	return (x < id) ? x : x - 1;
}

/**
* 发送消息
*/
int send(void * self, local_id dst, const Message * msg)
{
	PipeManager* from = (PipeManager*) self;
	
	if (dst == from->current_id)
	{
		return -1;
	}
	if (write(from->pipes[get_index(dst, from->current_id) * 2 + PIPE_WRITE_TYPE], 
		msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) < 0)
	{
		return -2;
	}
	return 0;
}

/**
* 发送广播
*/
int send_multicast(void * self, const Message * msg)
{
	PipeManager* from = (PipeManager*) self;
	local_id i;
	
	for (i = 0; i < from->total_ids; i++)
	{
		if (i == from->current_id)
		{
			continue;
		}
		while(send(from, i, msg) < 0);
	}
	return 0;
}

/**
* 接受消息
*/
int receive(void * self, local_id from, Message * msg)
{
	PipeManager* pm = (PipeManager*) self;
	
	if (from == pm->current_id)
	{
		return -1;
	}
	/* Read Header */
	if (read(pm->pipes[get_index(from, pm->current_id) * 2 + PIPE_READ_TYPE], msg, sizeof(MessageHeader)) < (int)sizeof(MessageHeader))
	{
		return -2;
	}
	
	/* Read Body */
	if (read(pm->pipes[get_index(from, pm->current_id) * 2 + PIPE_READ_TYPE], ((char*) msg) + sizeof(MessageHeader), msg->s_header.s_payload_len) < 0)
	{
		return -3;
	}
	return 0;
}

/**
* 接受广播消息
*/
int receive_any(void * self, Message * msg)
{
	PipeManager* pm = (PipeManager*) self;
	local_id i;
	
	for (i = 0; i < pm->total_ids; i++)
	{
		if (i == pm->current_id)
		{
			continue;
		}
		
		if (!receive(pm, i, msg))
		{
			return 0;
		}
	}
	return -1;
}
