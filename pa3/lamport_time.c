﻿#include "lamport_time.h"

static timestamp_t lamport_time = 0;

/**
* 增加时间戳
*/
timestamp_t increment_lamport_time()
{
    return ++lamport_time;
}

/**
* 设置逻辑时间
*/
timestamp_t set_lamport_time(timestamp_t new_lamport_time)
{
    if (lamport_time < new_lamport_time)
    {
        lamport_time = new_lamport_time;
    }
    return lamport_time;
}

/**
* 从消息中设置逻辑时间
*/
timestamp_t set_lamport_time_from_msg(Message* msg)
{
    set_lamport_time(msg->s_header.s_local_time);
    return increment_lamport_time();
}

/**
* 获得逻辑时间
*/
timestamp_t get_lamport_time()
{
    return lamport_time;
}
