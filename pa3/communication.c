#include "communication.h"
#include "logger.h"
#include "pa2345.h"
#include "ltime.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/** Set 0_NONBLOCK flag to fd
 *
 * @param fd			文件描述符
 *
 * @return -1 if can't get flags, -2 if can't set flags, 0 on success
 */
int set_nonblock(int pipe_id)
{
    int flags = fcntl(pipe_id, F_GETFL);
    if (!flags)
    {
        return ERROR_SET_NONBLOCK_NO_FLAGS;
    }
    flags = fcntl(pipe_id, F_SETFL, flags | O_NONBLOCK);
    if (!flags)
    {
        return ERROR_SET_NONBLOCK_NO_SET;
    }
    return RESULT_SET_NONBLOCK_SUCCESS;
}

/** 打开管道文件描述符
 *
 * @param proc_count    包含父进程的进程数量
 *
 * @return 管道文件描述符数组指针
 */
int* pipes_init(size_t proc_count)
{
    size_t offset = proc_count - 1;
    int* pipes = malloc(sizeof(int) * 2 * proc_count * (proc_count - 1));

    for (size_t i = 0; i < proc_count; i++)
    {
        for (size_t j = 0; j < proc_count; j++)
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

            if (set_nonblock(tmp_fd[0]) || set_nonblock(tmp_fd[1])) {
                return (int*)NULL;
            }
            pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_READ_TYPE] = tmp_fd[0];  /* READ */
            pipes[j * offset * 2 + (j > i ? i : i - 1) * 2 + PIPE_WRITE_TYPE] = tmp_fd[1]; /* WRITE */
        }
    }
    return pipes;
}

/** 初始化管道通讯
 *
 * @param pipes			管道文件描述符数组指针
 * @param proc_count    包含父进程的进程数量
 * @param curr_proc		当前进程本地ID
 *
 * @return 管道通讯对象指针
 */
PipesCommunication* communication_init(int* pipes, size_t proc_count, local_id curr_proc, balance_t balance) {
    PipesCommunication* this = malloc(sizeof(PipesCommunication));;
    size_t i, j;
    size_t offset = proc_count - 1;
    this->pipes = malloc(sizeof(int) * offset * 2);
    this->total_ids = proc_count;
    this->current_id = curr_proc;
    this->balance = balance;

    memcpy(this->pipes, pipes + curr_proc * 2 * offset, sizeof(int) * offset * 2);

    /* 关闭无用的文件描述符 */
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

/** 关闭所有文件描述符并释放资源
 *
 * @param pc		管道通讯对象指针
 */
void communication_release(PipesCommunication* pc)
{
    size_t i;
    for (i = 0; i < pc->total_ids - 1; i++)
    {
        close(pc->pipes[i * 2 + PIPE_READ_TYPE]);
        close(pc->pipes[i * 2 + PIPE_WRITE_TYPE]);
    }
    free(pc);
}

/** 发送事件消息给所有进程
 *
 * @param pc		管道通讯对象指针
 * @param type		消息类型: STARTED / DONE
 *
 * @return -1 非法消息类型, -2 内部错误, -3 发送消息错误, 0 成功
 */
int send_all_proc_event_msg(PipesCommunication* pc, MessageType type) {
    Message msg;
    uint16_t length = 0;
    char buf[MAX_PAYLOAD_LEN];

    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = type;
    msg.s_header.s_local_time = get_lamport_time();

    switch (type)
    {
    case STARTED:
        length = snprintf(buf, MAX_PAYLOAD_LEN, log_started_fmt, get_lamport_time(), pc->current_id, getpid(), getppid(), pc->balance);
        break;
    case DONE:
        length = snprintf(buf, MAX_PAYLOAD_LEN, log_done_fmt, get_lamport_time(), pc->current_id, pc->balance);
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

    send_multicast(pc, &msg);

    type == STARTED ? log_started(pc->current_id, pc->balance) : log_done(pc->current_id, pc->balance);

    return 0;
}

/** 发送停止消息给所有进程
 *
 * @param pc		管道通讯对象指针
 */
void send_all_stop_msg(PipesCommunication* pc)
{
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = 0;
    msg.s_header.s_type = STOP;
    msg.s_header.s_local_time = get_lamport_time();

    send_multicast(pc, &msg);
}

/** 发送转账消息
 *
 * @param pc		管道通讯对象指针
 * @param dst 		目标ID
 * @param order 	账单信息
 */
void send_transfer_msg(PipesCommunication* pc, local_id dst, TransferOrder* order) {
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = sizeof(TransferOrder);
    msg.s_header.s_type = TRANSFER;
    msg.s_header.s_local_time = get_lamport_time();

    memcpy(msg.s_payload, order, msg.s_header.s_payload_len);

    while (send(pc, dst, &msg) < 0);
}

/** 发送ACK
 *
 * @param pc		管道通讯对象指针
 * @param dst 		目标ID
 */
void send_ack_msg(PipesCommunication* pc, local_id dst) {
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = ACK;
    msg.s_header.s_local_time = get_lamport_time();
    msg.s_header.s_payload_len = 0;

    while (send(pc, dst, &msg) < 0);
}

/** 发送余额历史记录给父进程
 *
 * @param pc		管道通讯对象指针
 * @param dst 		目标ID
 * @param history	余额历史信息
 */
void send_balance_history(PipesCommunication* pc, local_id dst, BalanceHistory* bh)
{
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = sizeof(BalanceHistory);
    msg.s_header.s_type = BALANCE_HISTORY;
    msg.s_header.s_local_time = get_lamport_time();
    memcpy(msg.s_payload, bh, msg.s_header.s_payload_len);
    while (send(pc, dst, &msg) < 0);
}

/** 接收所有消息
 *
 * @param pc		管道通讯对象指针
 * @param type		消息类型
 */
void receive_all_msgs(PipesCommunication* pc, MessageType type)
{
    Message msg;

    for (local_id i = 1; i < pc->total_ids; i++)
    {
        if (i == pc->current_id)
        {
            continue;
        }
        while (receive(pc, i, &msg) < 0);

        set_lamport_time_from_msg(&msg);
    }

    switch (type)
    {
    case STARTED:
        log_received_all_started(pc->current_id);
        break;
    case DONE:
        log_received_all_done(pc->current_id);
        break;
    default:
        break;
    }
}
