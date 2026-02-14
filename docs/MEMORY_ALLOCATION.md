# Memory Allocation Structure of the OS

This document describes how memory is laid out and how heap allocation works in Galatea-NIX.

---

## 1. Overview

| Region / concept | Purpose |
|------------------|--------|
| **Kernel / .text.boot** | Entry and boot code (e.g. at 0x40000000 on QEMU) |
| **Heap** | Dynamic allocation via `mymalloc` / `myfree` (free-list allocator) |
| **Process shared memory** | Per-process region for threads in the same process |
| **Stacks** | Per-thread stacks (one per slot in `PROCS`) |

---

## 2. Heap (Dynamic Allocation)

### 2.1 Role

- **Heap** provides **heap memory** for the kernel and user tasks: arbitrary-size blocks, allocated with `mymalloc(size_in_words)` and freed with `myfree(ptr)`.
- Used for any runtime data that needs to outlive a single stack frame (e.g. message buffers, dynamic structures). Newlib’s `malloc` is not used; the OS uses this custom heap.

### 2.2 Implementation (Free-List with Coalescing)

- **Location**: The heap is a static array `heap_arena[HEAP_SIZE_DEFAULT]` in BSS (in `malloc.c`), so it lives in RAM. On QEMU virt, RAM is at 0x40000000+; a fixed address like 0x1000000 is not valid there, so the heap must be in BSS.
- **Initialization**: Called once at boot from `InitSys()` via `malloc_init_default()`, which calls `malloc_init((uint64_t)heap_arena, sizeof(heap_arena))`. That sets up a single free block covering the whole region:
  - First free block at start of arena: `[size in bytes][next pointer]` = `[heap_size, 0]`. The variable `freeListRoot` points to this block.
- **Block layout** (each block):
  - **Word 0**: Size of the block in **bytes** (including this header word).
  - **Word 1**: In free blocks: pointer to next free block (or 0). In allocated blocks: unused (payload starts after word 1).
  - **Word 2..**: Payload. `mymalloc` returns a pointer to the payload (i.e. block address + 16).
- **Size argument**: `mymalloc(size)` takes **size in words** (8-byte units). Internally it allocates `8 * (size + 1)` bytes (one extra word for the stored size).
- **Algorithm**:
  - **Allocation**: Walk the free list; find a block with `size >= 8*(size+1)`. If the remainder is large enough, split and leave a new free block; otherwise take the whole block. Update the free list and return payload pointer.
  - **Free**: Insert the block back into the free list (ordered by address). **Coalesce** with adjacent free blocks: if the next block in memory is free, merge; if the previous block is free, merge. So freeing can merge with both left and right neighbours.

### 2.3 API (layer1-processes)

- `void malloc_init_default(void);`  
  Call once at boot (in `InitSys`) before any `mymalloc`. Uses the BSS arena so the heap is in RAM.
- `void malloc_init(uint64_t heap_start, uint64_t heap_size);`  
  Low-level init; `malloc_init_default()` calls this with the BSS arena.
- `uint64_t *mymalloc(uint64_t size_in_words);`  
  Returns payload pointer or `NULL` if no suitable free block.
- `void myfree(uint64_t *ptr);`  
  Frees a block previously returned by `mymalloc`. No-op if `ptr` is `NULL`.

---

## 3. Process Shared Memory

- **Per-process region**: Each process has a shared memory region (base + size) in `PROCESS_CONTAINERS[]` and `PROCESS_SHARED_MEM[NPROCESSES][SHARED_MEM_PER_PROCESS]`.
- **Threads** in the same process share this region; they get it via `GetProcessSharedMem()` (syscall 14).
- This is **not** the heap: it is a fixed per-process area used for IPC and shared data (e.g. passing TIDs, spinlock test counters).

---

## 4. Stacks

- Each thread has a stack; the kernel sets `PROCS[i].stackpointer` (e.g. from a region like `STACKSTART + 0x10000 * (i+1)`).
- Stacks are **not** allocated from the heap; they come from a dedicated layout (e.g. one chunk per slot).

---

## 5. Layout Summary (Typical)

```
High addresses
  ...
  Process shared memory (per process)
  ...
  Heap (HEAP_ADDR .. HEAP_ADDR + HEAP_SIZE)
  ...
  Kernel BSS / data (PROCS, READY_QUEUE, Q-table, etc.)
  ...
  Kernel text
  ...
  Boot / entry
Low addresses (e.g. 0x40000000)
```

Exact addresses depend on the linker script and platform (QEMU vs RPI4).

---

## 6. Layer 1 Test

- **Test #5: Heap (mymalloc/myfree)** in `layer1-processes/tests/layer1_tests.c` checks:
  - `mymalloc(4)`, `mymalloc(8)` succeed.
  - Write/read-back (e.g. `0xDEADBEEF`, `0x12345678`).
  - `myfree` then `mymalloc(2)` (reuse).
  - Final read-back and `myfree` of all blocks.

This confirms the heap is working after `malloc_init()` is called from `InitSys()`.
