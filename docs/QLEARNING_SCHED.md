# Q-Learning 内核线程选择（符号化强化学习调度）

- **内存与 QEMU**：Q 表在内核中占用的整块内存、QEMU 下的布局与最佳实践见 **[QLEARNING_MEMORY.md](QLEARNING_MEMORY.md)**。
- **“每个线程都被记住”**：前 `QL_MAX_THREADS` 个线程（PID 1..8）在 Q 表中每个状态都有一列；先决条件、奖励、quantum 按线程索引配置。

## 设计概要

- **动作 (Action)**：每个线程即一个可选动作；调度器在就绪的 RL 线程中按 Q 值选择。
- **奖励 (Reward)**：线程**完成**（Exit）时发放奖励，并做 Q 表贝尔曼更新。
- **先决条件 (Prerequisites)**：部分线程必须在另一些线程完成后才能被选到；不满足先决条件的线程不会参与 Q 选择。
- **共享状态 (Shared state)**：符号状态 = “哪些 RL 线程已完成”的位图（`ql_completed_mask`），作为 Q(s,a) 的状态 s。
- **Quantum**：每个线程有 `quantum_ticks`（需投入的时间/时间片），当前用于元数据；可与时钟中断结合实现按 quantum 抢占。

## 关键文件

| 文件 | 作用 |
|------|------|
| `layer1-processes/qlearning_sched.h` | Q 表、线程元数据、API 声明 |
| `layer1-processes/qlearning_sched.c` | Q 表更新、先决条件检查、ε-greedy 选择 |
| `layer1-processes/syscall.c` | `scrPick` 中调用 Q 选择；Exit 时发奖励并调用 `ql_on_complete` |

## 状态与动作

- **状态空间**：`s ∈ [0, QL_NUM_STATES)`，其中 `QL_NUM_STATES = 2^QL_MAX_THREADS`（默认 256）。
  - 状态 s 的二进制位表示哪些 RL 任务（0..QL_MAX_THREADS-1）已完成。
- **动作空间**：`a ∈ [0, QL_MAX_THREADS)`，对应 PID 1..QL_MAX_THREADS 的线程。
- 只有 PID 1..QL_MAX_THREADS 参与 Q 学习；其余线程仍按原 min-heap（优先级/时间）调度。

## 先决条件与 Quantum

在 `qlearning_sched.h` 中，每个 RL 任务有：

```c
struct ql_thread_meta {
    uint32_t prereq_mask;     /* 必须先完成的任务集合（按位） */
    uint32_t quantum_ticks;  /* 时间片/需投入时间 */
    uint32_t heuristic_ticks;/* 预估运行时间（用于 knapsack / value-time） */
    uint32_t due_tick;       /* 截止时间：内核 runtime tick，0 = 无截止 */
    uint8_t  is_duty;        /* QL_THREAD_DUTY = 责任任务（逾期受罚），QL_THREAD_BOUNTY = 赏金任务（按时得奖） */
    int32_t  reward;         /* 赏金：按时完成时的奖励；责任：不用，逾期用 penalty */
    int32_t  penalty;        /* 责任任务：逾期时施加的惩罚（正数，实际给 -penalty） */
};
```

- **prereq_mask**：若任务 3 必须在任务 0、1 完成后才能运行，则 `prereq_mask = (1u<<0)|(1u<<1)`。
- **quantum_ticks**：可与时基结合，在调度/时钟里实现“最多运行 quantum 再切换”（当前仅存储，未参与调度逻辑）。
- **heuristic_ticks**：预估运行时间；用于 **Agent** 的 knapsack（权重）和 value/time 预算；若为 0 则用 `quantum_ticks`。
- **due_tick**：截止时间（内核运行 tick，相对 kernel start）；0 表示无截止。用于判断是否“按时完成”。
- **is_duty**：**赏金任务 (QL_THREAD_BOUNTY)**：按时完成得 reward，逾期得 0。**责任任务 (QL_THREAD_DUTY)**：按时完成无奖无罚，逾期得 **-penalty**（内核被惩罚）。
- **reward**：赏金任务的按时奖励；贝尔曼更新和 knapsack 的 value（赏金用 reward，责任用 penalty 作为“避免惩罚”的价值）。
- **penalty**：仅责任任务；逾期时有效奖励 = -penalty。

设置方式（例如在 `InitSys` 或首次创建该任务后）：

```c
/* ql_set_thread_meta(thread_index, prereq_mask, quantum_ticks, heuristic_ticks, reward); */
ql_set_thread_meta(0, 0, 100, 80, 1);   /* 任务 0：赏金，reward=1 */
ql_set_thread_meta(1, 1u<<0, 50, 40, 2);
ql_set_thread_meta(2, (1u<<0)|(1u<<1), 80, 60, 3);

/* ql_set_thread_due(thread_index, due_tick, is_duty, penalty); due_tick=0 表示无截止 */
ql_set_thread_due(0, 0, QL_THREAD_BOUNTY, 0);           /* 赏金任务，无截止 */
ql_set_thread_due(1, 50000, QL_THREAD_BOUNTY, 0);      /* 赏金，需在 runtime 50000 前完成 */
ql_set_thread_due(2, 100000, QL_THREAD_DUTY, 10);      /* 责任任务，逾期惩罚 10（实际 -10 给 Q 更新） */
```

完成时内核用 **ql_effective_reward(pid, current_runtime_ticks)** 计算有效奖励（赏金：按时=reward、逾期=0；责任：按时=0、逾期=-penalty），再调用 **ql_on_complete(pid, effective_reward)**。

## 启用/关闭

在 `syscall.c` 中：

```c
#define USE_QL_SCHED 1   /* 1=Q 学习调度，0=仅原 min-heap */
```

设为 0 则完全使用原来的按优先级/时间的 min-heap 调度。

## Layer 1 Agent：规划、背包、时间预算二分

调度器在选线程时先由 **Agent** 做“规划”：用 **knapsack（0/1）** 在时间预算内最大化价值，并用 **二分查找** 确定当前批次的 **最优时间预算**（最大化 value/time）。

1. **初始化**：`InitSys` 中调用 `ql_init_layer1()`，会做 `ql_init()` 并为每个 RL 任务设置默认 `heuristic_ticks`、`quantum_ticks`、`reward`（可之后用 `ql_set_thread_meta` 覆盖）。
2. **每线程启发式**：`ql_thread_meta[i].heuristic_ticks` 表示任务 i 的预估运行时间（ticks）；knapsack 中作为“重量”，与 reward（价值）一起用于优化。
3. **Knapsack**：当前就绪且满足先决条件的任务作为物品，重量 = `heuristic_ticks`（或 `quantum_ticks`）；价值：赏金任务 = `reward`，责任任务 = `penalty`（按时完成“避免惩罚”的价值）；在给定时间预算 B 内做 0/1 背包，得到最优子集。
4. **二分查找预算**：在 `[1, QL_AGENT_MAX_BUDGET]` 上二分（并局部扫描）找时间 T，使 `value(T)/T` 最大，即当前批次的最优 value/time 预算。
5. **选线程**：`scrPick` 先调用 `ql_agent_plan(ready_pids, n)`；若返回有效 pid 则从 ready 堆中移除并调度；否则退回到 `ql_pick_ready`（Q 值）或原 min-heap。

宏 `QL_AGENT_MAX_BUDGET`（默认 2048 ticks）限制二分与背包的预算上界。

## Q 更新（整数定点）

- Q 值用 `int32_t`，定点缩放 `QL_FIXED_SHIFT=10`。
- 更新公式：`Q(s,a) += alpha * (r + gamma * max_a' Q(s',a') - Q(s,a))`，其中 `alpha=QL_ALPHA/1024`，`gamma=QL_GAMMA/1024`。
- 内核中无浮点运算，全部整数运算。

## 扩展：共享内存

当前“共享”的符号信息只有完成位图 `ql_completed_mask`。若需要更丰富的共享状态（例如共享缓冲区、符号寄存器），可以：

- 在 `qlearning_sched` 中增加一块 `ql_shared_mem[]` 或类似结构；
- 在状态编码中把部分共享内存摘要编进 s（例如 hash 或位段），以区分不同“共享状态”下的 Q(s,a)。

这样即可在现有“线程 = 动作、完成 = 奖励、先决条件 + quantum”的框架上，做成更完整的符号强化学习内核。

---

## 相关文档

| 文档 | 内容 |
|------|------|
| [QLEARNING_MEMORY.md](QLEARNING_MEMORY.md) | Q 表是否工作、每个线程如何在 Q 表中被记住、整块内核内存分配、QEMU 架构下的布局与最佳实践。 |
