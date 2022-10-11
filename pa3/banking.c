#include "banking.h"
#include "communication.h"
#include "logger.h"
#include "lamporttime.h"


/**
* 转账函数
* 处理转账事务
*
* @param parent_data 父进程数据指针
* @param src 源进程id
* @param dst 目标进程id
* @param amount 金额
*/
void transfer(void* parent_data, local_id src, local_id dst, balance_t amount)
{
    Message msg;
    PipesCommunication* parent = (PipesCommunication*)parent_data;
    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;
    //1. 增加时间戳
    increase_lamport_time();
    //2. 发送转账请求消息
    send_transfer_msg(parent, src, &order);
    //3. 记录转出
    log_transfer_out(src, dst, amount);
    //4. 发送转账接受消息
    while (receive(parent, dst, &msg) < 0 || msg.s_header.s_type != ACK);
    //5. 从消息中设置时间
    set_lamport_time_from_msg(&msg);
    //6. 记录转入
    log_transfer_in(src, dst, amount);
}
