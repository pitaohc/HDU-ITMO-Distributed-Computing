#include "ipm.h"
#include "communication.h"

#include <unistd.h>

enum {
    IPC_SEND_TRANSFORM_ERROR = -2,
    IPC_SEND_SAME_ERROR = -1,
    IPC_SEND_SUCCESS = 0
};
enum {
    IPC_RECEIVE_TRANSFORM_ERROR = -2,
    IPC_RECEIVE_SAME_ERROR = -1,
    IPC_RECEIVE_SUCCESS = 0
};


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
int send(void* self, local_id dst, const Message* msg)
{
    PipeManager* pm = (PipeManager*)self;

    //如果接收方==发送方，则返回错误编码
    if (dst == pm->currentId)
    {
        return IPC_SEND_SAME_ERROR;
    }

    if (write(pm->pipes[GET_INDEX(dst, pm->currentId) * 2 + PIPE_TYPE_WRITE], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) < 0) {
        return IPC_SEND_TRANSFORM_ERROR;
    }
    //int pipe = pm->pipes[GET_INDEX(dst, pm->currentId)] * 2 + PIPE_TYPE_WRITE;
    //if (write(pipe, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len < 0))
    //{
    //    return IPC_SEND_TRANSFORM_ERROR;
    //}
    return IPC_SEND_SUCCESS;
}

/**
* 发送广播
*/
int send_multicast(void* self, const Message* msg)
{
    PipeManager* pm = (PipeManager*)self;
    for (local_id i = 0; i < pm->ids_count; ++i)
    {
        if (i != pm->currentId)
        {
            int res = send(self, i, msg); //接收单次发送的结果
            if (res != IPC_SEND_SUCCESS)
            {
                return res; //如果出错则传递结果
            }
        }
    }
    return IPC_SEND_SUCCESS;
}

/**
* 接受消息
*/
int receive(void* self, local_id from, Message* msg)
{
    PipeManager* pm = (PipeManager*)self;

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
    if (read(pm->pipes[get_index(from, pm->current_id) * 2 + PIPE_READ_TYPE], ((char*)msg) + sizeof(MessageHeader), msg->s_header.s_payload_len) < 0)
    {
        return -3;
    }
    return 0;
}

/**
* 接受广播消息
*/
int receive_any(void* self, Message* msg)
{
    PipeManager* pm = (PipeManager*)self;
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
