# Dynamic Memory (Heap Allocator)

This document describes the **dynamic memory** system in Galatea-NIX: the kernel heap allocator that provides `malloc` / `free` and per-process virtual RAM mapping.

---

## 1. What Is Dynamic Memory?

- **Dynamic memory** is runtime allocation from a **heap**: the kernel reserves a fixed region (the heap arena) and hands out variable-size blocks on request. Blocks can be freed and reused.
- It is **not** the same as:
  - **Process shared memory** — fixed per-process region (GetProcessSharedMem); not allocated from the heap.
  - **Stacks** — per-thread, fixed layout; not from the heap.
- On bare metal there is no C library heap; the kernel **overrides** `malloc` and `free` with this implementation.

---

## 2. Where It Lives

| Item | Location |
|------|----------|
| **Heap arena** | BSS: `heap_arena[HEAP_SIZE_DEFAULT]` in `layer1-processes/malloc/malloc.c`. Lives in RAM (e.g. 0x40000000+ on QEMU virt). |
| **Free list** | In-place in the arena: each free block holds `[size][next]`; `freeListRoot` points to the first free block. |
| **Allocation map** | Static array `alloc_map[MALLOC_MAP_ENTRIES]` in `malloc.c`; one entry per allocation (process_id, vram, phys, size). |
| **Per-process VRAM cursor** | `next_vram[NPROCESSES]` in `malloc.c`; next virtual address to assign per process. |

---

## 3. Block Layout

Each block in the heap (allocated or free) has:

- **Word 0**: Size in **bytes** (including this header word).
- **Word 1**: In **free** blocks: pointer to next free block (or 0). In **allocated** blocks: unused.
- **Word 2..**: Payload. `malloc` / `malloc_for_process` return a pointer to the **payload** (block address + 16 bytes).

Size is requested in **words** (8-byte units). Internally the allocator reserves `8 * (size + 1)` bytes (one extra word for the stored size).

---

## 4. Allocation Algorithm

- **Free list** is kept **sorted by ascending address** (smallest first).
- **Allocate**: Take the **first** free block that fits (first fit = smallest address). If the remainder is large enough, **split** the block and leave a new free block; otherwise use the whole block. Remove the block from the free list and return the payload pointer.
- **Free**: Insert the block back into the free list in **sorted order** and **coalesce** with adjacent free blocks (merge left and/or right).

This “smallest-address-first” policy (SmallestInfiniteSet-style) reduces fragmentation by filling low addresses first.

---

## 5. Per-Process Virtual RAM (VRAM) and Mapping

Every allocation is recorded with three properties:

| Property | Meaning |
|----------|---------|
| **Process ID** | Which process owns the reservation. |
| **VRAM address** | Payload address in that process’s virtual heap range. |
| **Physical RAM address** | Block header (and payload) in the kernel heap. |

- Each process has a **virtual heap range**: `VRAM_HEAP_BASE + process_id * VRAM_HEAP_SIZE_PER_PROCESS` (see `config.h`).
- On **allocate**: the kernel assigns the next VRAM address in that process’s range, allocates a physical block, and stores `(process_id, vram_payload, phys_start, size_bytes)` in the map.
- **Without MMU**: the allocator returns the **physical** payload pointer so dereference works; the map still holds the triple for bookkeeping and for a future MMU.
- **Resolve VRAM → physical**: use `malloc_vram_to_phys(process_id, vram_payload)` to get the physical payload pointer for a given process’s VRAM address.

---

## 6. API (layer1-processes)

| Function | Purpose |
|----------|---------|
| `void malloc_init_default(void)` | Initialize heap from BSS arena. Call once at boot (e.g. from `InitSys`) before any allocation. |
| `void malloc_init(uint64_t heap_start, uint64_t heap_size)` | Low-level init; `malloc_init_default()` uses this with the BSS arena. |
| `uint64_t *malloc(uint64_t size_in_words)` | Allocate for process 0 (init/kernel). Returns payload pointer or `NULL`. |
| `uint64_t *malloc_for_process(int process_id, uint64_t size_in_words)` | Allocate for the given process; reserves VRAM and records (process_id, vram, phys). Returns physical payload pointer or `NULL`. |
| `void free(uint64_t *ptr)` | Free a block previously returned by `malloc` or `malloc_for_process`. No-op if `ptr` is `NULL`. |
| `uint64_t *malloc_vram_to_phys(int process_id, uint64_t vram_payload)` | Resolve a process’s VRAM (payload) address to the physical payload pointer. Returns `NULL` if not found. |

**Kernel helper**: `GetCurrentProcessId()` (in `syscall.c`) returns the current thread’s process id for use with `malloc_for_process(GetCurrentProcessId(), size)`.

---

## 7. Configuration (config.h)

| Constant | Meaning |
|----------|---------|
| `HEAP_SIZE_DEFAULT` | Heap arena size in bytes (e.g. 64 KB). |
| `MALLOC_MAP_ENTRIES` | Max number of allocation map entries (concurrent allocations). |
| `VRAM_HEAP_BASE` | Base of per-process virtual heap (e.g. 0x10000). |
| `VRAM_HEAP_SIZE_PER_PROCESS` | Virtual heap size per process (e.g. 64 KB). |

---

## 8. Implementation Files

- **Header**: `layer1-processes/malloc/malloc.h`
- **Implementation**: `layer1-processes/malloc/malloc.c`
- **Initialization**: called from `InitSys()` in `layer1-processes/syscall.c` via `malloc_init_default()`.

---

## 9. Tests

- **Test #5 (Heap)**: `layer1-processes/tests/layer1_tests.c` — `malloc_for_process`, `free`, reuse, read-back.
- **Test #9 (Memory isolation)**: Same file — distinct blocks per allocation, data isolated.

---

## 10. See Also

- **[MEMORY_ALLOCATION.md](MEMORY_ALLOCATION.md)** — Full memory layout (heap, process shared memory, stacks) and detailed algorithm.
- **[structure.md](structure.md)** — Repository layout and where `malloc` lives.
