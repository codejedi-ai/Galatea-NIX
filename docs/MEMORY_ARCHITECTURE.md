# Memory Architecture & Distribution

## Overview
The CS452 microkernel on Pi 4 uses a **tiered, static-allocation** memory model optimized for bare-metal embedded systems. No virtual address spaces — each task has:
- Private stack (statically allocated)
- Shared heap (global malloc pool)
- Shared kernel data structures (IPC queues, PCBs)

```
┌─────────────────────────────────────────────────┐
│ Pi 4 RAM (1 GB = 0x3E000000, kernel uses ~256MB)│
├─────────────────────────────────────────────────┤
│ Kernel image (loaded at 0x80000)                │ ← 512 KB
├─────────────────────────────────────────────────┤
│ Stack pool (20 tasks × 64 KB = 1.28 MB)        │ Task 0:    0x80000 + 64KB
├─────────────────────────────────────────────────┤ Task 1:    0x80000 + 128KB
│ Kernel heap (malloc, shared by all tasks)      │ Task 2:    0x80000 + 192KB
│ - Grows upward from HEAP_START                 │ ...
│ - malloc_init_default() initializes at boot    │ Task 19:   0x80000 + 1.28MB
├─────────────────────────────────────────────────┤
│ PMM (physical page frame bitmap + pool)        │
│ - 4 KB page allocator                          │
│ - ~4 MB (1024 pages) available to kernel       │
├─────────────────────────────────────────────────┤
│ Device memory (UART, GIC, Timer @ 0xFE0...)   │
└─────────────────────────────────────────────────┘
```

## Memory Allocation Tiers

### 1. Stack (Per-Task, Static)
```c
// config.h
#define NUMPROCS              20      // max task IDs
#define STACK_SIZE_PER_THREAD 0x10000 // 64 KB per task

// syscall.c - task creation
PROCS[p].stackpointer = ((uint8_t*)STACKSTART) + (STACK_SIZE_PER_THREAD * (p + 1));
```

**Properties:**
- Static at compile time (BSS segment)
- Non-shrinking (no guard pages)
- **Full isolation per task** — one task's stack overflow → corrupt adjacent task
- Grows **downward** (ARM calling convention): SP initialized at `STACKSTART + (n+1) * STACK_SIZE`

### 2. Heap (Global, Shared)
```c
// malloc/malloc.h
void malloc_init_default(void);  // init heap from BSS
uint64_t *mymalloc(uint64_t size_in_words);
```

**Properties:**
- Single arena in BSS, initialized once at boot
- **All tasks share one heap** — no per-task isolation
- Word-granule allocation (8-byte units)
- Used by:
  - Kernel services (IPC queues, data structures)
  - Games (canvas buffers, snake body arrays)
  - EventBus (topic/subscription structures)
- **Risk**: Memory leak in one task fragments heap for all tasks; one task's malloc can indirectly starve others

### 3. Physical Memory (PMM)
```c
// pmm.h
void  *pmm_alloc_page(void);           // alloc 1× 4 KB frame
void  *pmm_alloc_pages(size_t n);      // alloc n contiguous frames
uint64_t pmm_free_pages_count(void);   // stats
```

**Properties:**
- Bitmap-based allocator for 4 KB page frames
- ~4 MB pool available (1024 pages)
- Used for:
  - Large contiguous buffers (mmap-like regions)
- Kernel can query fragmentation via `pmm_free_pages_count()`

### 4. Kernel Structures (Static)
```c
// syscall.c
static struct PCB PROCS[NUMPROCS];           // 20 task control blocks
static struct heap_t pidheap;                // free PID heap
static struct queue BLOCKED_LIST[NUMPROCS];  // per-task IPC waiters
```

All in BSS, allocated once. No dynamic limits (bounded at compile time).

## Memory Distribution by Component

### Task Stacks
```
Task ID → Memory Range (assuming 64 KB each)
0        → 0x80000 + 64K  = 0x90000
1        → 0x80000 + 128K = 0xA0000
...
19       → 0x80000 + 1.28M = 0x140000
```

Each stack is **isolated** — if task 5's function recurses infinitely, it corrupts task 6's stack, not the shared heap.

### Shared Heap Layout (Dynamic)
```
HEAP_START (from config.h, usually after stacks + kernel code)
├─ malloc slab(s)
├─ game canvas buffers (Snake board, Tetris well)
├─ EventBus topics/subscriptions
├─ IPC message queues
└─ [free space]
```

**Allocation pattern:**
- malloc calls `mymalloc(size_in_words)` for chunks
- Returns words (8-byte units), not bytes
- Word boundaries enforced (size rounded up)

### PCB Pool
```c
struct PCB {
    uint64_t *pcpointer;           // task entry point
    uint64_t *stackpointer;        // top of stack (grows down)
    int pid;                       // 1-20 (not 0-19)
    int parentpid;                 // parent task ID
    int process_id;                // address space ID (8 max)
    uint8_t priority;              // 0-99
    uint8_t pstate;                // running/ready/blocked
    int waiting_reply;             // syscall reply pending
    int waiting_send;              // waiting to send IPC
    int waiting_recieve_head;      // blocked on Receive()
};

PROCS[NUMPROCS] = {20 × sizeof(struct PCB)} // ~5 KB total
```

## IPC & Shared Memory

### Message Passing (Send/Receive/Reply)
```c
// layer2-messaging/messaging.h
struct message {
    int sender;
    uint64_t *msg;
    int msglen;
};

// Per-task receive queue
queue_t PROCS[i].waiting_recieve_head;  // circular queue of pending messages
```

Tasks communicate by copying message pointers. **No deep copy** — the message buffer itself is in sender's stack or shared heap, receiver gets a pointer.

### EventBus (Shared Topics)
```c
// layer3-services/eventbus.c
static EventSubscription subscriptions[128];  // shared queue array
// Each subscription has:
//   int queue[256];  // circular buffer for that subscriber
//   int head, tail;
```

All subscriptions live in kernel BSS; each gets 256 message slots (= ~2 KB per subscription).

## Current Limitations & Future Work

### ❌ No Address Space Isolation
- All tasks see the same memory
- One task's buffer overflow → corrupt any other task
- Privilege separation is logical (syscall checks), not memory-based

### ❌ Single Shared Heap
- One malloc pool for all tasks
- No per-task memory quotas
- Fragmentation affects all tasks equally
- Memory leak in one task starves others

### ✅ Per-task Heap (Logical Isolation)
- Each task gets its own 256 KB heap slice from a PMM pool
- `task_malloc()` allocates from the current task's slice
- Isolation is logical: no shared-pointer aliasing between slices

## Memory Queries

**Current tools to inspect memory:**
```bash
shell> mem                    # heap stats (total/used/free B)
shell> pages                  # frame pool stats (used/free frames)
shell> htop                   # per-task memory (indicative, not real)
```

## Example: Snake's Memory Use

Snake allocates:
```c
static int bx[SMAX];           // snake body x coords (1200 ints = 9.6 KB)
static int by[SMAX];           // snake body y coords (1200 ints = 9.6 KB)
// Total: ~19.2 KB of stack + shared heap

// When running:
ScreenBuf sb;                  // local stack var
screenbuf_init(&sb, 40, 120);  // allocates 2 × 60×200 cell arrays
//    = 2 × (60 × 200 × sizeof(cell)) = 2 × 120 KB = 240 KB in heap
```

Snake's worst-case footprint: **20 KB (statics) + 240 KB (buffer) ≈ 260 KB**.

With 4 MB heap and multiple tasks running, fragmentation is the main risk (no garbage collection, no compaction).

## Recommendations

1. **Monitor heap health**: Log `malloc_free_bytes()` periodically to catch leaks
2. **Bound task allocations**: Add per-task malloc quotas in future (subheaps)
3. **Avoid large static arrays**: Use malloc'd buffers instead (easier to track)
5. **Test memory pressure**: Fill heap deliberately, verify graceful failure
