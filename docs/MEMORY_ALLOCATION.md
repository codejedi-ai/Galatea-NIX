# Memory Allocation Structure of the OS

This document describes how memory is laid out and how heap allocation works in Galatea-NIX.

---

## 1. Overview

| Region / concept | Purpose |
|------------------|--------|
| **Kernel / .text.boot** | Entry and boot code (e.g. at 0x40000000 on QEMU) |
| **Heap** | Dynamic allocation via `malloc` / `free` (free-list allocator; bare-metal override) |
| **Process shared memory** | Per-process region for threads in the same process |
| **Stacks** | Per-thread stacks (one per slot in `PROCS`) |

---

## 2. Heap (Dynamic Allocation)

### 2.1 Role

- **Heap** provides **heap memory** for the kernel and user tasks: arbitrary-size blocks, allocated with `malloc(size_in_words)` and freed with `free(ptr)`. On bare metal there is no C library heap; the kernel overrides with this allocator.
- Used for any runtime data that needs to outlive a single stack frame (e.g. message buffers, dynamic structures). Newlib’s `malloc` is not used; the OS uses this custom heap.

### 2.2 Implementation (Free-List with Coalescing)

- **Location**: The heap is a static array `heap_arena[HEAP_SIZE_DEFAULT]` in BSS (in `malloc.c`), so it lives in RAM. On QEMU virt, RAM is at 0x40000000+; a fixed address like 0x1000000 is not valid there, so the heap must be in BSS.
- **Initialization**: Called once at boot from `InitSys()` via `malloc_init_default()`, which calls `malloc_init((uint64_t)heap_arena, sizeof(heap_arena))`. That sets up a single free block covering the whole region:
  - First free block at start of arena: `[size in bytes][next pointer]` = `[heap_size, 0]`. The variable `freeListRoot` points to this block.
- **Block layout** (each block):
  - **Word 0**: Size of the block in **bytes** (including this header word).
  - **Word 1**: In free blocks: pointer to next free block (or 0). In allocated blocks: unused (payload starts after word 1).
  - **Word 2..**: Payload. `malloc` returns a pointer to the payload (i.e. block address + 16).
- **Size argument**: `malloc(size)` takes **size in words** (8-byte units). Internally it allocates `8 * (size + 1)` bytes (one extra word for the stored size).
- **Algorithm**:
  - **Allocation**: Free list is kept **sorted by ascending address**. Walk the list and take the **first** block that fits (smallest address that fits). If the remainder is large enough, split and leave a new free block; otherwise take the whole block. Update the free list and return payload pointer.
  - **Free**: Insert the block back into the free list in **sorted order** (addBack). **Coalesce** with adjacent free blocks: if the next block in memory is free, merge; if the previous block is free, merge. So freeing can merge with both left and right neighbours.
- **Defragmentation (SmallestInfiniteSet-style)**: Always allocating from the smallest available address and adding freed blocks back in sorted order keeps free space compact at the high end and reduces fragmentation.
- **Per-process virtual RAM (VRAM) and mapping**: Every process uses its own virtual heap range; every memory reservation has three properties:
  - **Process ID**: which process owns the reservation.
  - **VRAM address**: the payload address in that process’s virtual space (within `VRAM_HEAP_BASE + process_id * VRAM_HEAP_SIZE_PER_PROCESS`).
  - **Physical RAM address**: the block header (and payload) in the kernel heap.
  The map stores this triple so that VRAM under a process is mapped to physical RAM. Without an MMU, `malloc_for_process` returns the physical payload pointer so dereference works; the map still holds the triple for bookkeeping and for a future MMU. Use `malloc_vram_to_phys(process_id, vram_payload)` to resolve a process’s VRAM address to a physical pointer.

### 2.3 API (layer1-processes)

- `void malloc_init_default(void);`  
  Call once at boot (in `InitSys`) before any alloc. Uses the BSS arena so the heap is in RAM.
- `void malloc_init(uint64_t heap_start, uint64_t heap_size);`  
  Low-level init; `malloc_init_default()` calls this with the BSS arena.
- `uint64_t *malloc(uint64_t size_in_words);`  
  Allocates for process 0 (init/kernel). Returns physical payload pointer or `NULL`. Bare-metal override (no C library heap).
- `uint64_t *malloc_for_process(int process_id, uint64_t size_in_words);`  
  Allocates for the given process; reserves VRAM in that process’s range and records (process_id, vram, phys). Returns physical payload pointer (so dereference works without MMU) or `NULL`.
- `void free(uint64_t *ptr);`  
  Frees a block previously returned by `malloc` or `malloc_for_process`. No-op if `ptr` is `NULL`.
- `uint64_t *malloc_vram_to_phys(int process_id, uint64_t vram_payload);`  
  Resolves a process’s VRAM (payload) address to the physical payload pointer. Returns `NULL` if not found.

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

- **Test #5: Heap (malloc/free)** in `layer1-processes/tests/layer1_tests.c` checks:
  - `malloc_for_process(pid, 4)`, `malloc_for_process(pid, 8)` succeed.
  - Write/read-back (e.g. `0xDEADBEEF`, `0x12345678`).
  - `free` then `malloc_for_process(pid, 2)` (reuse).
  - Final read-back and `free` of all blocks.

This confirms the heap is working after `malloc_init()` is called from `InitSys()`.
