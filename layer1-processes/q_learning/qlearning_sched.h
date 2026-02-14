#ifndef _QLEARNING_SCHED_H_
#define _QLEARNING_SCHED_H_ 1

#include <stdint.h>
#include "config.h"

#define QL_NUM_STATES     (1u << QL_MAX_THREADS)
#define QL_QTABLE_ENTRIES (QL_NUM_STATES * QL_MAX_THREADS)
#define QL_QTABLE_BYTES   (QL_QTABLE_ENTRIES * sizeof(int32_t))

struct ql_thread_meta {
	uint32_t prereq_mask;
	uint32_t quantum_ticks;
	uint32_t heuristic_ticks;
	uint32_t due_tick;
	uint8_t  is_duty;
	int32_t  reward;
	int32_t  penalty;
};
#define QL_META_BYTES     (QL_MAX_THREADS * sizeof(struct ql_thread_meta))

extern uint32_t ql_completed_mask;
extern int32_t ql_Q[QL_NUM_STATES][QL_MAX_THREADS];
extern struct ql_thread_meta ql_thread_meta[QL_MAX_THREADS];

void ql_init(void);
uint32_t ql_get_state(void);
int ql_prereqs_met(uint32_t s, unsigned a);
int ql_pick_ready(int *pids, unsigned n);
void ql_on_complete(int pid, int32_t reward);
int32_t ql_effective_reward(int pid, uint32_t current_runtime_ticks);
void ql_record_action(uint32_t s, unsigned a);
void ql_set_thread_meta(unsigned thread_index, uint32_t prereq_mask, uint32_t quantum_ticks, uint32_t heuristic_ticks, int32_t reward);
void ql_set_thread_due(unsigned thread_index, uint32_t due_tick, uint8_t is_duty, int32_t penalty);
int ql_pid_to_index(int pid);
void ql_init_layer1(void);
int ql_agent_plan(int *ready_pids, unsigned n);

#endif
