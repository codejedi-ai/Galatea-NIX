#ifndef _QLEARNING_SCHED_H_
#define _QLEARNING_SCHED_H_ 1

#include <stdint.h>

/*
 * Symbolic Q-Learning Kernel Scheduler
 *
 * - Each thread is an action (selection). Completing a thread yields a reward.
 * - Some threads have prerequisites (must run after others complete).
 * - Shared memory: symbolic state = which threads have completed (bitmask).
 * - Each thread has a quantum (time investment / max time slice).
 *
 * State  = completed set (bitmask over RL threads)
 * Action = which RL thread to run next (only if ready and prereqs met)
 * Q(s,a) updated on transition: reward on completion, Bellman update.
 */

#define QL_MAX_THREADS      8
#define QL_NUM_STATES     (1u << QL_MAX_THREADS)   /* 256 */
#define QL_QTABLE_ENTRIES (QL_NUM_STATES * QL_MAX_THREADS)
/* Entire chunk for Q-table (kernel-allocated at link time, in BSS): */
#define QL_QTABLE_BYTES   (QL_QTABLE_ENTRIES * sizeof(int32_t))  /* 8192 with defaults */
#define QL_FIXED_SHIFT    10
#define QL_ALPHA          32   /* learning rate * 1024 */
#define QL_GAMMA          896  /* discount * 1024 (~0.875) */
#define QL_EPSILON        128  /* explore 12.5% of the time * 1024 */

/* Thread type: bounty = reward for on-time; duty = penalty for late */
#define QL_THREAD_BOUNTY 0
#define QL_THREAD_DUTY   1

/* Per-thread metadata for RL threads (PID 1..QL_MAX_THREADS map to index 0..QL_MAX_THREADS-1) */
struct ql_thread_meta {
	uint32_t prereq_mask;     /* bitmask: which threads must be completed before this one (index bits) */
	uint32_t quantum_ticks;  /* time slice / time to invest (e.g. timer ticks) */
	uint32_t heuristic_ticks;/* estimate of how long this thread takes (for knapsack / value-time) */
	uint32_t due_tick;       /* deadline: kernel runtime tick by which thread must complete; 0 = no deadline */
	uint8_t  is_duty;        /* QL_THREAD_DUTY = duty (penalized if late), QL_THREAD_BOUNTY = bounty (reward if on time) */
	int32_t  reward;         /* bounty: reward when on time; duty: unused for reward, use penalty when late */
	int32_t  penalty;        /* duty only: penalty (positive) applied when completed after due_tick */
};
#define QL_META_BYTES     (QL_MAX_THREADS * sizeof(struct ql_thread_meta))


/* Agent: knapsack + binary search on time budget for optimal value/time */
#define QL_AGENT_MAX_BUDGET 2048  /* max time budget (ticks) for binary search */

/* Shared symbolic state: which RL threads have completed (bitmask) */
extern uint32_t ql_completed_mask;

/* Q-table: Q[state][action]. Fixed-point (QL_FIXED_SHIFT). */
extern int32_t ql_Q[QL_NUM_STATES][QL_MAX_THREADS];

/* Per-thread metadata (index = RL thread index 0..QL_MAX_THREADS-1). */
extern struct ql_thread_meta ql_thread_meta[QL_MAX_THREADS];

void ql_init(void);

/* Encode current completed set as state index (0 .. QL_NUM_STATES-1). */
uint32_t ql_get_state(void);

/* Return 1 if thread with RL index 'a' has its prerequisites satisfied in state 's'. */
int ql_prereqs_met(uint32_t s, unsigned a);

/* Among ready RL PIDs (pids[] of length n), choose action by Q(s,a); return chosen pid or -1. */
int ql_pick_ready(int *pids, unsigned n);

/* Call when thread (pid) completes: pass effective reward (use ql_effective_reward). Then Q-update and advance state. */
void ql_on_complete(int pid, int32_t reward);

/* Compute effective reward/penalty for thread on completion. current_runtime_ticks = kernel runtime (e.g. get_timerLO() - kernelStartTime).
 * Bounty: reward if on time (current <= due_tick or no deadline), else 0. Duty: 0 if on time, else -penalty. */
int32_t ql_effective_reward(int pid, uint32_t current_runtime_ticks);

/* Record (s, a) when picking a thread so Bellman update can run on completion. */
void ql_record_action(uint32_t s, unsigned a);

/* Optional: set metadata for RL thread index (0..QL_MAX_THREADS-1). heuristic_ticks: 0 => use quantum_ticks. */
void ql_set_thread_meta(unsigned thread_index, uint32_t prereq_mask, uint32_t quantum_ticks, uint32_t heuristic_ticks, int32_t reward);

/* Set due time and type: due_tick = 0 means no deadline. is_duty: QL_THREAD_BOUNTY or QL_THREAD_DUTY. penalty used for duty when late. */
void ql_set_thread_due(unsigned thread_index, uint32_t due_tick, uint8_t is_duty, int32_t penalty);

/* Map kernel PID to RL thread index; return -1 if not an RL thread. */
int ql_pid_to_index(int pid);

/* ========== Layer 1 Agent: plan ahead, knapsack, binary-search budget ========== */
/* Initialize Q-learning + agent defaults (thread heuristics, etc.). Call from InitSys. */
void ql_init_layer1(void);

/* Agent plans optimal thread run: uses binary search to find time budget, then knapsack to pick subset.
 * ready_pids[] = kernel PIDs (1..QL_MAX_THREADS), n = count. Returns chosen pid or -1. */
int ql_agent_plan(int *ready_pids, unsigned n);

#endif
