/*
 * Symbolic Q-Learning Kernel Scheduler
 *
 * State  = bitmask of completed RL threads (0 .. QL_NUM_STATES-1)
 * Action = RL thread index 0 .. QL_MAX_THREADS-1
 * Q(s,a) updated on thread completion: reward + gamma * max_a' Q(s',a') - Q(s,a)
 * All arithmetic is integer fixed-point (no floating point in kernel).
 */

#include "qlearning_sched.h"
#include <stddef.h>

uint32_t ql_completed_mask = 0;
int32_t  ql_Q[QL_NUM_STATES][QL_MAX_THREADS];
struct ql_thread_meta ql_thread_meta[QL_MAX_THREADS];

/* Last (s, a) for Bellman update when we observe reward (s' = new state). */
static uint32_t ql_last_s = 0;
static unsigned ql_last_a = 0;
static uint8_t  ql_have_last = 0;

void ql_init(void)
{
	ql_completed_mask = 0;
	ql_have_last = 0;
	for (uint32_t s = 0; s < QL_NUM_STATES; s++)
		for (unsigned a = 0; a < QL_MAX_THREADS; a++)
			ql_Q[s][a] = 0;
	for (unsigned i = 0; i < QL_MAX_THREADS; i++) {
		ql_thread_meta[i].prereq_mask = 0;
		ql_thread_meta[i].quantum_ticks = 0;
		ql_thread_meta[i].heuristic_ticks = 0;
		ql_thread_meta[i].due_tick = 0;
		ql_thread_meta[i].is_duty = QL_THREAD_BOUNTY;
		ql_thread_meta[i].reward = 1;
		ql_thread_meta[i].penalty = 0;
	}
}

uint32_t ql_get_state(void)
{
	return ql_completed_mask & (QL_NUM_STATES - 1);
}

int ql_prereqs_met(uint32_t s, unsigned a)
{
	if (a >= QL_MAX_THREADS) return 0;
	uint32_t need = ql_thread_meta[a].prereq_mask;
	return (s & need) == need;
}

int ql_pid_to_index(int pid)
{
	if (pid < 1 || (unsigned)(pid - 1) >= QL_MAX_THREADS) return -1;
	return pid - 1;
}

/* Simple LCG for epsilon-greedy (no rand in kernel). */
static unsigned ql_seed = 12345;
static unsigned ql_rand_u16(void)
{
	ql_seed = ql_seed * 1103515245u + 12345u;
	return (ql_seed >> 16) & 0x7fff;
}

/* Pick among ready RL PIDs; pids[] are kernel PIDs (1..QL_MAX_THREADS). Returns chosen pid or -1. */
int ql_pick_ready(int *pids, unsigned n)
{
	uint32_t s = ql_get_state();
	int best_pid = -1;
	int32_t best_q = -2147483647 - 1; /* INT32_MIN */

	for (unsigned i = 0; i < n; i++) {
		int pid = pids[i];
		int a = ql_pid_to_index(pid);
		if (a < 0) continue;
		if (!ql_prereqs_met(s, (unsigned)a)) continue;
		int32_t q = ql_Q[s][(unsigned)a];
		if (q > best_q) {
			best_q = q;
			best_pid = pid;
		}
	}

	/* Epsilon-greedy: with probability epsilon pick random ready action */
	if (best_pid >= 0 && n > 0) {
		unsigned r = ql_rand_u16() & 0x3ffu;  /* 0..1023 */
		if (r < (QL_EPSILON * 1024u >> QL_FIXED_SHIFT)) { /* ~12.5% */
			unsigned idx = ql_rand_u16() % n;
			int pid = pids[idx];
			int a = ql_pid_to_index(pid);
			if (a >= 0 && ql_prereqs_met(s, (unsigned)a))
				best_pid = pid;
		}
	}

	return best_pid;
}

/* Max over a' of Q(s', a') for next state s'. */
static int32_t ql_max_Q(uint32_t s_prime)
{
	int32_t m = ql_Q[s_prime][0];
	for (unsigned a = 1; a < QL_MAX_THREADS; a++) {
		int32_t q = ql_Q[s_prime][a];
		if (q > m) m = q;
	}
	return m;
}

void ql_on_complete(int pid, int32_t reward)
{
	int a = ql_pid_to_index(pid);
	if (a < 0) return;

	uint32_t s_prime = ql_completed_mask | (1u << (unsigned)a);
	if (s_prime >= QL_NUM_STATES) s_prime &= (QL_NUM_STATES - 1);

	/* Bellman update for last (s, a) if we have it */
	if (ql_have_last) {
		int32_t max_next = ql_max_Q(s_prime);
		/* TD = r + gamma * max_a' Q(s',a') - Q(s,a); Q(s,a) += alpha * TD */
		int32_t q_sa = ql_Q[ql_last_s][ql_last_a];
		int32_t td = reward + (int32_t)((QL_GAMMA * (int64_t)max_next) >> QL_FIXED_SHIFT) - q_sa;
		ql_Q[ql_last_s][ql_last_a] = q_sa + (int32_t)((QL_ALPHA * (int64_t)td) >> QL_FIXED_SHIFT);
		ql_have_last = 0;
	}

	ql_completed_mask = s_prime;
}

/* Record (s, a) so that when this thread completes we do Q(s,a) update. Call from scheduler when picking. */
void ql_record_action(uint32_t s, unsigned a)
{
	ql_last_s = s;
	ql_last_a = a;
	ql_have_last = 1;
}

void ql_set_thread_meta(unsigned thread_index, uint32_t prereq_mask, uint32_t quantum_ticks, uint32_t heuristic_ticks, int32_t reward)
{
	if (thread_index >= QL_MAX_THREADS) return;
	ql_thread_meta[thread_index].prereq_mask = prereq_mask & (QL_NUM_STATES - 1);
	ql_thread_meta[thread_index].quantum_ticks = quantum_ticks;
	ql_thread_meta[thread_index].heuristic_ticks = heuristic_ticks;
	ql_thread_meta[thread_index].reward = reward;
}

void ql_set_thread_due(unsigned thread_index, uint32_t due_tick, uint8_t is_duty, int32_t penalty)
{
	if (thread_index >= QL_MAX_THREADS) return;
	ql_thread_meta[thread_index].due_tick = due_tick;
	ql_thread_meta[thread_index].is_duty = is_duty;
	ql_thread_meta[thread_index].penalty = penalty;
}

int32_t ql_effective_reward(int pid, uint32_t current_runtime_ticks)
{
	int a = ql_pid_to_index(pid);
	if (a < 0) return 0;
	const struct ql_thread_meta *t = &ql_thread_meta[(unsigned)a];
	if (t->due_tick == 0) {
		/* No deadline: bounty gets reward, duty gets 0 */
		return t->is_duty ? 0 : t->reward;
	}
	if (current_runtime_ticks <= t->due_tick) {
		/* On time: bounty gets reward, duty gets 0 (no penalty) */
		return t->is_duty ? 0 : t->reward;
	}
	/* Late: bounty gets 0, duty gets -penalty */
	return t->is_duty ? (-t->penalty) : 0;
}

/* ========== Agent: knapsack (0/1) + binary search on time budget ========== */

/* Weight = heuristic (or quantum), value = reward. Only ready threads (prereqs met). */
static uint32_t ql_heuristic_ticks(unsigned a)
{
	uint32_t h = ql_thread_meta[a].heuristic_ticks;
	return h != 0 ? h : ql_thread_meta[a].quantum_ticks;
}

/* Value for knapsack: bounty = reward (if > 0); duty = penalty (value of completing on time = avoiding penalty). */
static int32_t ql_knapsack_value_for_thread(unsigned a)
{
	const struct ql_thread_meta *t = &ql_thread_meta[a];
	if (t->is_duty)
		return t->penalty > 0 ? t->penalty : 0;
	return t->reward > 0 ? t->reward : 0;
}

/* 2D knapsack for correct backtrack: dp[b][i] = max value with budget b, items 0..i. */
static int32_t ql_knapsack_chosen(uint32_t budget, const unsigned *ready_indices, unsigned n,
	unsigned *chosen_out, unsigned *chosen_len)
{
	static int32_t dp[QL_AGENT_MAX_BUDGET + 1][QL_MAX_THREADS];
	const uint32_t max_b = (budget <= QL_AGENT_MAX_BUDGET) ? budget : QL_AGENT_MAX_BUDGET;

	for (uint32_t b = 0; b <= max_b; b++)
		for (unsigned i = 0; i < QL_MAX_THREADS; i++)
			dp[b][i] = 0;

	for (unsigned i = 0; i < n; i++) {
		unsigned a = ready_indices[i];
		uint32_t w = ql_heuristic_ticks(a);
		int32_t v = ql_knapsack_value_for_thread(a);
		for (uint32_t b = 0; b <= max_b; b++) {
			int32_t skip = (i > 0) ? dp[b][i - 1] : 0;
			int32_t take = (b >= w) ? ((i > 0 ? dp[b - w][i - 1] : 0) + v) : -1;
			dp[b][i] = (take > skip) ? take : skip;
		}
	}

	*chosen_len = 0;
	uint32_t b = max_b;
	for (int i = (int)n - 1; i >= 0; i--) {
		if (b == 0) break;
		unsigned a = ready_indices[(unsigned)i];
		uint32_t w = ql_heuristic_ticks(a);
		int32_t v = ql_knapsack_value_for_thread(a);
		int32_t cur = dp[b][(unsigned)i];
		int32_t skip_val = (i > 0) ? dp[b][(unsigned)(i - 1)] : 0;
		if (b >= w && cur != skip_val && cur == (i > 0 ? dp[b - w][(unsigned)(i - 1)] : 0) + v) {
			chosen_out[(*chosen_len)++] = a;
			b -= w;
		}
	}
	return n > 0 ? dp[max_b][n - 1] : 0;
}

/* Value only (no chosen set) for binary search. */
static int32_t ql_knapsack_value(uint32_t budget, const unsigned *ready_indices, unsigned n)
{
	unsigned dummy[QL_MAX_THREADS];
	unsigned dlen;
	return ql_knapsack_chosen(budget, ready_indices, n, dummy, &dlen);
}

/* Binary search on time budget T in [1, max_budget] to maximize value(T)/T (optimal value/time). */
static uint32_t ql_agent_binary_search_budget(const unsigned *ready_indices, unsigned n)
{
	uint32_t lo = 1;
	uint32_t hi = QL_AGENT_MAX_BUDGET;
	if (n == 0) return 1;

	while (lo + 4 < hi) {
		uint32_t mid = lo + (hi - lo) / 2;
		int32_t v_mid = ql_knapsack_value(mid, ready_indices, n);
		int32_t v_mid1 = ql_knapsack_value(mid + 1, ready_indices, n);
		/* Ratio = value / time; avoid div by zero */
		int32_t r_mid = (mid > 0) ? (v_mid * 1024 / (int32_t)(int)mid) : 0;
		int32_t r_mid1 = (mid + 1 > 0) ? (v_mid1 * 1024 / (int32_t)(int)(mid + 1)) : 0;
		if (r_mid1 > r_mid)
			lo = mid + 1;
		else
			hi = mid;
	}
	/* Find best T in [lo, hi] */
	uint32_t best_t = lo;
	int32_t best_ratio = 0;
	for (uint32_t t = lo; t <= hi && t <= QL_AGENT_MAX_BUDGET; t++) {
		int32_t v = ql_knapsack_value(t, ready_indices, n);
		int32_t r = (t > 0) ? (v * 1024 / (int32_t)(int)t) : 0;
		if (r > best_ratio) {
			best_ratio = r;
			best_t = t;
		}
	}
	return best_t;
}

/* Agent: plan ahead with knapsack + binary search; return chosen pid (first in optimal set). */
int ql_agent_plan(int *ready_pids, unsigned n)
{
	uint32_t s = ql_get_state();
	unsigned ready_indices[QL_MAX_THREADS];
	unsigned n_ready = 0;
	for (unsigned i = 0; i < n && n_ready < QL_MAX_THREADS; i++) {
		int pid = ready_pids[i];
		int a = ql_pid_to_index(pid);
		if (a >= 0 && ql_prereqs_met(s, (unsigned)a))
			ready_indices[n_ready++] = (unsigned)a;
	}
	if (n_ready == 0) return -1;

	uint32_t budget = ql_agent_binary_search_budget(ready_indices, n_ready);
	unsigned chosen[QL_MAX_THREADS];
	unsigned chosen_len = 0;
	ql_knapsack_chosen(budget, ready_indices, n_ready, chosen, &chosen_len);
	if (chosen_len == 0) return -1;
	/* Return first chosen thread as PID (index -> pid = index+1) */
	return (int)(chosen[0] + 1);
}

void ql_init_layer1(void)
{
	ql_init();
	/* Default thread meta: heuristic = 50*(i+1), reward = i+1, no prereqs. */
	for (unsigned i = 0; i < QL_MAX_THREADS; i++) {
		uint32_t heuristic = 50u * (i + 1);
		uint32_t quantum = heuristic + 10u;
		ql_set_thread_meta(i, 0, quantum, heuristic, (int32_t)(int)(i + 1));
	}
}
