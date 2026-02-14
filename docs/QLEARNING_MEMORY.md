# Q-Learning Kernel: Memory Allocation and QEMU Layout

## Does the Q-Learning Kernel Work?

Yes. The Q-learning path is active when `USE_QL_SCHED` is 1 in `syscall.c`:

- **Selection**: `scrPick()` collects ready RL threads (PID 1..QL_MAX_THREADS), calls `ql_pick_ready()` to choose by Q(s,a) (with ε-greedy), removes that PID from the ready heap, records (s,a) for the Bellman update, and returns it.
- **Reward and update**: When a thread calls `Exit()`, the kernel calls `ql_on_complete(pid, reward)` before `Kill(p)`, which applies the Bellman update for the last (s,a) and advances the completed-state mask.
- **Prerequisites**: Only threads whose `prereq_mask` is satisfied by the current `ql_completed_mask` are considered in `ql_pick_ready()`.

So the Q-learning kernel path is wired end-to-end; behaviour depends on `ql_thread_meta` (rewards, prereqs) and the Q-table.

---

## Every Thread Remembered for the Q-Table

The Q-table explicitly remembers **each of the first `QL_MAX_THREADS` threads** (by index):

| Concept | Implementation |
|--------|------------------|
| **State** | Bitmask of which RL threads have **completed**: `s = ql_completed_mask` (each bit = one thread index 0..QL_MAX_THREADS−1). |
| **Action** | Which RL thread to run next: `a ∈ [0, QL_MAX_THREADS)` ↔ PID 1..QL_MAX_THREADS. |
| **Q(s,a)** | One entry per (state, action): `ql_Q[s][a]` = value of running thread `a` when completed set is `s`. |

So:

- **State space**: `s ∈ [0, QL_NUM_STATES)` with `QL_NUM_STATES = 2^QL_MAX_THREADS`. Each of the 2^N bit patterns corresponds to a subset of “which of the N threads have completed.”
- **Action space**: Exactly `QL_MAX_THREADS` actions, one per RL thread. Each thread has a dedicated column in the Q-table for every state.
- **Thread ↔ index**: PID 1 → index 0, PID 2 → index 1, … , PID `QL_MAX_THREADS` → index `QL_MAX_THREADS−1`. `ql_pid_to_index(pid)` and the per-thread metadata `ql_thread_meta[i]` (prereqs, quantum, reward) are defined for each of these indices.

So every RL thread is “remembered” in the Q-table (every state has an entry for that thread) and in the thread metadata. Threads with PID &gt; QL_MAX_THREADS are not part of the Q-table; they are scheduled by the original min-heap policy.

---

## Entire Chunk of Memory Allocated to the Kernel for the Q-Table

The Q-learning subsystem uses a **single contiguous region of kernel memory** for the Q-table and related data. Nothing is dynamically allocated at runtime; everything is link-time static (BSS/data).

### Size of the chunk

| Symbol / region | Type | Size (bytes) | Description |
|----------------|------|--------------|-------------|
| `ql_Q` | BSS | `QL_NUM_STATES × QL_MAX_THREADS × sizeof(int32_t)` | Q-table: 256×8×4 = **8192** |
| `ql_thread_meta` | BSS | `QL_MAX_THREADS × sizeof(struct ql_thread_meta)` | Per-thread prereq, quantum, reward: 8×12 = **96** |
| `ql_completed_mask` | BSS | 4 | Current state (completed-set bitmask). |
| `ql_last_s`, `ql_last_a`, `ql_have_last` | BSS | ~9 | Last (s,a) for Bellman update. |
| `ql_seed` | data | 4 | LCG seed for ε-greedy. |

**Total Q-learning chunk** (current defaults): **~8.3 KB** (dominated by `ql_Q`).

Formulas (from `qlearning_sched.h`):

```c
#define QL_MAX_THREADS      8
#define QL_NUM_STATES     (1u << QL_MAX_THREADS)   /* 256 */

/* Q-table size in bytes */
QL_NUM_STATES * QL_MAX_THREADS * sizeof(int32_t)   /* 8192 */

/* Task metadata size in bytes */
QL_MAX_THREADS * sizeof(struct ql_thread_meta)       /* 96 */
```

So the **entire chunk** the kernel gives to the Q-learning subsystem is this fixed block (Q-table + thread meta + small state). No extra allocator is required.

---

## QEMU Architecture: Where This Lives and Best Practice

### QEMU `virt` memory map (relevant part)

- **RAM**: Starts at `0x4000_0000`. Kernel is loaded here (linker origin in `linker.ld`).
- **MMIO**: Below RAM (e.g. GIC at 0x0800_0000, UART at 0x0900_0000, etc.).

The kernel image is placed at the linker origin (e.g. `0x4000_0000`); `.text.boot` first, then typically `.text`, `.rodata`, `.data`, `.bss`. The Q-table and all Q-learning globals are in **`.bss`** (or `.data` for `ql_seed`), so they lie **inside this kernel RAM region** at fixed offsets from the load address.

Example from a build (addresses will vary with compiler/linker):

- `ql_Q` at `0x40009858`, size `0x2000` (8 KB).
- `ql_thread_meta` at `0x4000b858`, size `0x60` (96 B).
- Other `ql_*` symbols nearby in the same kernel image.

So on QEMU, the “entire chunk” for the Q-table **is** the kernel’s own static region in RAM; no separate device or carve-out is required.

### Best practice for QEMU (and in general)

1. **Keep the Q-table in BSS (or a dedicated section)**  
   One contiguous block (Q-table + metadata) at link time is simple, predictable, and does not fragment kernel memory. No `malloc` in the kernel for RL.

2. **Reserve enough for the table**  
   With `QL_MAX_THREADS = 8`, the table is 8 KB. If you increase `QL_MAX_THREADS` to N:
   - States: `2^N`
   - Q-table size: `2^N × N × 4` bytes (e.g. N=10 → 40 KB, N=12 → 192 KB). Ensure kernel RAM on QEMU is large enough (e.g. `-m` in QEMU).

3. **Alignment**  
   The linker/compiler will align globals; `int32_t` Q-table needs no special alignment beyond normal. If you add a dedicated section later, 4- or 8-byte alignment is enough.

4. **No MMIO for the Q-table**  
   The Q-table is ordinary RAM in the kernel image; it should **not** be placed in MMIO space. Keeping it in `.bss` (or a kernel RAM section) satisfies this.

5. **Single copy, no paging**  
   On this bare-metal kernel there is no paging; the Q-table has one physical mapping in kernel RAM, which is what you want for a simple “entire chunk allocated to the kernel.”

---

## Summary Table

| Topic | Answer |
|-------|--------|
| Does the Q-learning kernel work? | Yes: selection by Q(s,a), reward on Exit, Bellman update, prerequisites. |
| Is every thread remembered for the Q-table? | Yes, for RL threads: each of the first `QL_MAX_THREADS` threads has an action index and a column in Q(s,a) for every state. |
| Entire chunk for Q-table? | Yes: one static block (~8.3 KB with default N=8): `ql_Q` + `ql_thread_meta` + small state. |
| QEMU: where is it? | In kernel RAM at 0x4000_0000+ (same as rest of kernel BSS/data). |
| QEMU: best practice? | Keep Q-table in BSS (or one link-time section), size it with `2^N × N × 4` if you change N, ensure RAM size is sufficient. |

For more on behaviour (rewards, prerequisites, quantum), see **QLEARNING_SCHED.md**.
