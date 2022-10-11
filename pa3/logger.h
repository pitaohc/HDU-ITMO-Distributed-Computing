#ifndef __IFMO_DISTRIBUTED_CLASS_LOGGER__H
#define __IFMO_DISTRIBUTED_CLASS_LOGGER__H

#include "ipc.h"
#include "communication.h"
#include "banking.h"


void log_init();
void log_destroy();

void log_pipes(const PipeManager* pm);

void log_started(const local_id id, const balance_t balance);
void log_received_all_started(const local_id id);
void log_done(const local_id id, const balance_t balance);
void log_received_all_done(const local_id id);

void log_transfer_out(const local_id from, const local_id dst, const balance_t amount);
void log_transfer_in(const local_id from, const local_id dst, const balance_t amount);

#endif
