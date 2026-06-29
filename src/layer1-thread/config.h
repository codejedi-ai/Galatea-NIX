/*
 * Central configuration: no hardcoded numbers in other files.
 * Include this where kernel/scheduler/heap/test constants are needed.
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_ 1

/* ----- Kernel / process limits ----- */
#define QUEUESIZE              255
#define NUMPROCS               20
#define NPROCESSES             8
#define SHARED_MEM_PER_PROCESS 4096
#define MAXINT                 2147483647
#define MININT                 (-2147483648)
#define MAXEVENT               1025

/* ----- Interrupt / UART IDs ----- */
#define CLOCKINTID  99
#define UARTINTER   153
#define RITC        6
#define TXIC        5
#define RXIC        4
#define CTSMIM      1

/* ----- Debug / scheduler ----- */
#define DEBUG       5
#define DEBUG_EXIT  0

/* ----- Stack (per-thread stack size in bytes) ----- */
#define STACK_SIZE_PER_THREAD  0x10000

/* ----- Heap ----- */
#define HEAP_SIZE_DEFAULT  (64 * 1024)  /* 64 KB */
#define MALLOC_MAP_ENTRIES 64           /* max concurrent allocations for phys↔virtual map */
/* Per-process virtual heap: each process gets its own VRAM range (process_id → vram base). */
#define VRAM_HEAP_BASE              0x10000UL
#define VRAM_HEAP_SIZE_PER_PROCESS  0x10000UL  /* 64 KB virtual heap per process */

/* ----- Main / idle ----- */
#define CLOCKSERVERON  1
#define UARTSERVERON   1

/* ----- Layer 1 tests ----- */
#define TEST_LEVEL_SUITE   0
#define TEST_LEVEL_TEST    1
#define TEST_LEVEL_LINE    2
#define TEST_INDENT_SPACES_PER_LEVEL  2
#define SPINLOCK_TEST_ITERS  50
#define TEST_MAGIC_VAL   0x12345678
#define TEST_MAGIC_VAL2  0xDEADBEEF

#endif
