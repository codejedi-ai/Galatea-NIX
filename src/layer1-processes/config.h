/*
 * Central configuration: no hardcoded numbers in other files.
 * Include this where kernel/scheduler/heap/test constants are needed.
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_ 1

/* ----- Kernel / process limits ----- */
#define QUEUESIZE              255
#define NUMPROCS               64
#define NPROCESSES             8
#define SHARED_MEM_PER_PROCESS 4096
#define MAXINT                 2147483647
#define MININT                 (-2147483648)
#define MAXEVENT               1025

/* ----- Interrupt / UART IDs ----- */
/* Kernel tick = ARM generic timer EL1 physical timer, GIC PPI 30 (per-CPU).
 * (The old BCM2711 system-timer SPI 99 never fired under QEMU raspi4b.) */
#define CLOCKINTID  30
#define UARTINTER   153
#define RITC        6
#define TXIC        5
#define RXIC        4
#define CTSMIM      1

/* ----- Debug / scheduler ----- */
#define DEBUG       5
#define DEBUG_EXIT  0
/* USE_QL_SCHED: SHELVED (set to 0). The Q-learning scheduler code is kept in the
 * tree (layer1-processes/q_learning/, the ql_* call sites, docs/QLEARNING_*.md)
 * but compiled out, so the kernel uses the plain priority min-heap scheduler.
 * Set back to 1 to re-enable the experiment. Do not delete the Q-learning code. */
#define USE_QL_SCHED 0

/* ----- Stack (per-thread stack size in bytes) ----- */
#define STACK_SIZE_PER_THREAD  0x10000

/* ----- Heap (disabled — DarcyOS APU uses stack-only memory) ----- */
#define HEAP_SIZE_DEFAULT  0
#define HEAP_SIZE_PER_TASK 0

/* ----- Virtual memory / physical frame allocator ----- */
#define PAGE_SIZE        4096
#define PAGE_SHIFT       12
/* PMM pool: 128 MB physical frame pool (covers 64 × 256 KB task heaps + BRK). */
#define PMM_POOL_PAGES   32768           /* 32768 * 4KB = 128 MB page pool */
#define BRK_REGION_PAGES 256             /* 256 * 4KB = 1 MB reserved for ksbrk */

/* ----- Q-learning scheduler ----- */
#define QL_MAX_THREADS      8
#define QL_FIXED_SHIFT      10
#define QL_ALPHA            32    /* learning rate * 1024 */
#define QL_GAMMA            896   /* discount * 1024 (~0.875) */
#define QL_EPSILON          128   /* explore 12.5% * 1024 */
#define QL_THREAD_BOUNTY    0
#define QL_THREAD_DUTY      1
#define QL_AGENT_MAX_BUDGET 2048  /* max time budget (ticks) for binary search */

/* ----- Main / idle ----- */
#define CLOCKSERVERON  1   /* on: GIC + generic timer + name/clock servers (CS452 K3) */
#define UARTSERVERON   1
/* Marklin train-control server.  Speaks the binary Marklin Digital protocol.
 * MARKLIN_HW_UART3 selects the transport — override at compile time:
 *   0 (default, QEMU): AUX mini-UART (serial1); no patched QEMU needed.
 *   1 (real Pi hat):   PL011 UART3 @ 0xFE201600, 2400 baud → Marklin 6051 box.
 * Pass -DMARKLIN_HW_UART3=1 for real-Pi builds (or set in Makefile). */
#define ENABLE_MARKLIN   1
#ifndef MARKLIN_HW_UART3
#define MARKLIN_HW_UART3 0  /* 0=AUX/QEMU  1=UART3/real-Pi */
#endif

/* Link server: AUX mini-UART bridge. Disabled (AUX carries Marklin in QEMU). */
#define ENABLE_LINK    0

/* ----- UI / game canvas (Layer 4/5) -----
 * Compile-time STORAGE caps for the resizable game canvas: they dimension the
 * UiCanvas backing array and the snake body, so they must be macros (array
 * bounds), not runtime values. The ACTIVE size is picked at runtime — autofit to
 * the terminal, the `screen` command, and +/- — these are just the maximum it
 * can grow to. Override per build with e.g. -DUI_CANVAS_WMAX=160. */
#ifndef UI_CANVAS_WMAX
#define UI_CANVAS_WMAX   200    /* max canvas width  (cols) */
#endif
#ifndef UI_CANVAS_HMAX
#define UI_CANVAS_HMAX   96     /* max canvas height (rows); Tetris uses BH*CELL */
#endif
#ifndef UI_CANVAS_HMIN
#define UI_CANVAS_HMIN   8      /* min playable canvas height */
#endif
#ifndef UI_CANVAS_CHROME
#define UI_CANVAS_CHROME 6      /* frame rows around the canvas (title/rules/status) */
#endif

/* ----- Boot behavior ----- */
#define BOOT_RUN_TESTS 0   /* terminal product — no boot test suite */
#define START_SHELL    1   /* UART terminal shell after boot */

/* ----- Layer 1 tests ----- */
#define TEST_LEVEL_SUITE   0
#define TEST_LEVEL_TEST    1
#define TEST_LEVEL_LINE    2
#define TEST_INDENT_SPACES_PER_LEVEL  2
#define SPINLOCK_TEST_ITERS  50
#define TEST_MAGIC_VAL   0x12345678
#define TEST_MAGIC_VAL2  0xDEADBEEF

/* ----- Board / hardware configuration -----
 * Defined once in config.c (the central home for hardware config). The values
 * are still mirrored by the compile-time macros above and in mmio_config.h,
 * which C requires for #if conditions, array sizes, and static initialisers;
 * BOARD is the single runtime object the rest of the OS reads. Guarded for
 * assembly, which also includes this header. */
#ifndef __ASSEMBLER__
#include <stdint.h>

/* ----- App / terminal design size (single source of truth) -----
 * Default design terminal: 176×50 (+20 cols from 156×50).
 * Body = screen minus UI chrome. UI_DESIGN_MIN_COLS fits the train ASCII art.
 * Keep UI_DESIGN_METER_ROWS / UI_DESIGN_DIV_CHROME in sync with ui_elem.h. */
#ifndef UI_DESIGN_SCREEN_COLS
#define UI_DESIGN_SCREEN_COLS   176
#endif
#ifndef UI_DESIGN_SCREEN_ROWS
#define UI_DESIGN_SCREEN_ROWS   50
#endif
#define UI_DESIGN_MIN_COLS      96
#define UI_DESIGN_BASE_W        16
#define UI_DESIGN_BASE_H        10
#define UI_DESIGN_METER_ROWS     1
#define UI_DESIGN_DIV_CHROME     6
#define UI_DESIGN_BODY_H        (UI_DESIGN_SCREEN_ROWS - UI_DESIGN_METER_ROWS \
                                   - UI_DESIGN_DIV_CHROME)
#define UI_DESIGN_BODY_W        (UI_DESIGN_SCREEN_COLS > 2 \
                                   ? UI_DESIGN_SCREEN_COLS - 2 : UI_DESIGN_SCREEN_COLS)

typedef struct {
	const char *platform;              /* board name */
	uintptr_t   mmio_base;             /* peripheral window base */
	uintptr_t   uart0_base;            /* PL011 console (serial0) */
	uintptr_t   uart3_base;            /* PL011 Marklin line (real Pi 4 only) */
	uintptr_t   aux_base;              /* AUX mini-UART = container link (serial1) */
	uintptr_t   gic_base;              /* GIC-400 base */
	uintptr_t   gpio_base;             /* GPIO */
	unsigned    console_baud;          /* serial0 baud */
	unsigned    marklin_baud;          /* link baud */
	unsigned    link_port;             /* host-side serial1 TCP port (vhw bridge) */
} board_config_t;

extern const board_config_t BOARD;
#endif /* __ASSEMBLER__ */

#endif
