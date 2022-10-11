#ifndef __IFMO_DISTRIBUTED_CLASS_LAMPORTTIME__H
#define __IFMO_DISTRIBUTED_CLASS_LAMPORTTIME__H

#include "ipc.h"

const timestamp_t get_lamport_time();
const timestamp_t increase_lamport_time();
const timestamp_t set_lamport_time(const timestamp_t new_lamport_time);
const timestamp_t set_lamport_time_from_msg(const Message* msg);

#endif
