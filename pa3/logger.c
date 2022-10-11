#include <stdio.h>
#include <unistd.h>

#include "pa2345.h"
#include "logger.h"
#include "common.h"


FILE* pipes_log_file, * events_log_file;

/**
* 初始化日志
*/
void log_init()
{
    pipes_log_file = fopen(pipes_log, "w");
    events_log_file = fopen(events_log, "w");
}

/**
* 释放日志
*/
void log_destroy()
{
    if (pipes_log_file) //如果管道日志存在
    {
        fclose(pipes_log_file); //释放日志
        pipes_log_file = NULL; //指向NULL
    }
    if (pipes_log_file) //如果事件日志存在
    {
        fclose(events_log_file);
        events_log_file = NULL;
    }
}

/**
* 记录管道创建
*/
void log_pipes(const PipeManager * pm)
{
    if (pipes_log_file == NULL)
    {
        fprintf(stderr, "Please init pipes log file\n");
        return;
    }

    size_t i;

    fprintf(pipes_log_file, "Process %d pipes:\n", pm->current_id);

    for (i = 0; i < pm->total_ids; i++)
    {
        if (i == pm->current_id)
        {
            continue;
        }

        fprintf(pipes_log_file, "Process %ld\tRead pipe %d\tWrite pipe%d \n", i, pm->pipes[(i < pm->current_id ? i : i - 1) * 2 + PIPE_READ_TYPE],
            pm->pipes[(i < pm->current_id ? i : i - 1) * 2 + PIPE_WRITE_TYPE]);
    }
    fprintf(pipes_log_file, "\n");
}

/**
* 记录初始余额
*/
void log_started(const local_id id, const balance_t balance)
{
    if (events_log_file == NULL)
    {
        fprintf(stderr, "Please init events log file\n");
        return;
    }

    fprintf(events_log_file, log_started_fmt, get_lamport_time(), id, getpid(), getppid(), balance);
    printf(log_started_fmt, get_lamport_time(), id, getpid(), getppid(), balance);
}


/**
* 记录接收到所有进程就绪消息
*/
void log_received_all_started(const local_id id)
{
    if (events_log_file == NULL)
    {
        fprintf(stderr, "Please init events log file\n");
        return;
    }
    fprintf(events_log_file, log_received_all_started_fmt, get_lamport_time(), id);
    printf(log_received_all_started_fmt, get_lamport_time(), id);
}

/**
* 记录子进程完成
*/
void log_done(const local_id id, const balance_t balance)
{
    if (events_log_file == NULL)
    {
        fprintf(stderr, "Please init events log file\n");
        return;
    }
    fprintf(events_log_file, log_done_fmt, get_lamport_time(), id, balance);
    printf(log_done_fmt, get_lamport_time(), id, balance);
}

/**
* 记录接受所有进程完成
*/
void log_received_all_done(const local_id id)
{
    if (events_log_file == NULL)
    {
        fprintf(stderr, "Please init events log file\n");
        return;
    }
    fprintf(events_log_file, log_received_all_done_fmt, get_lamport_time(), id);
    printf(log_received_all_done_fmt, get_lamport_time(), id);
}

/**
* 记录转出
*/
void log_transfer_out(const local_id from, const local_id dst, const balance_t amount)
{
    if (events_log_file == NULL)
    {
        fprintf(stderr, "Please init events log file\n");
        return;
    }
    fprintf(events_log_file, log_transfer_out_fmt, get_lamport_time(), from, amount, dst);
    printf(log_transfer_out_fmt, get_lamport_time(), from, amount, dst);
}

/**
* 记录转入
*/
void log_transfer_in(const local_id from, const local_id dst, const balance_t amount)
{
    if (events_log_file == NULL)
    {
        fprintf(stderr, "Please init events log file\n");
        return;
    }
    fprintf(events_log_file, log_transfer_in_fmt, get_lamport_time(), dst, amount, from);
    printf(log_transfer_in_fmt, get_lamport_time(), dst, amount, from);
}
