#include "banking.h"
#include "communication.h"
#include "log3pa.h"
#include "ltime.h"


/**
* ת�˺���
* ����ת������
* 
* @param parent_data ����������ָ��
* @param src Դ����id
* @param dst Ŀ�����id
* @param amount ���
*/
void transfer(void * parent_data, local_id src, local_id dst, balance_t amount)
{
	Message msg;
	PipesCommunication* parent = (PipesCommunication*) parent_data;
	TransferOrder order;
	order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;
	//1. ����ʱ���
	increment_lamport_time();
	//2. ����ת��������Ϣ
    send_transfer_msg(parent, src, &order);
	//3. ��¼ת��
	log_transfer_out(src, dst, amount);
	//4. ����ת�˽�����Ϣ
    while (receive(parent, dst, &msg) < 0 || msg.s_header.s_type != ACK);
	//5. ����Ϣ������ʱ��
	set_lamport_time_from_msg(&msg);
	//6. ��¼ת��
	log_transfer_in(src, dst, amount);		
}
