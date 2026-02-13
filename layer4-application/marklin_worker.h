#ifndef _MCC_h_
#define _MCC_h_
#include "../syscall.h"
#define TRIGGERED 0
#define RELEASED 1
#define SWITCH_COUNT 18


#define PRINTSWITCHCOL 1
#define PRINTSWITCHROW 28
struct free_task_list{
    uint32_t data[NUMPROCS];
    uint32_t tail;
    uint32_t size;
};

void marklin_worker();
void marklin_worker_read_notifier();
void set_solonoid(int marklin_worker_tid, uint8_t sol_id, char state);
void set_train_state(int marklin_worker_tid, uint8_t train_ind, char speed);
#endif
