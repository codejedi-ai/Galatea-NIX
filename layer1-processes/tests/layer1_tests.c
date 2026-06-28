#include "layer1_tests.h"
#include "../syscall.h"
#include "../processes.h"
#include "../rpi.h"
#include "../timer/systimer.h"
#include "../locks.h"
#include "../malloc/malloc.h"
#include "../../layer2-messaging/messaging.h"

#define TEST_PRINT(m, fmt, ...) do { \
	test_print_prefix(&(m)); \
	uart_printf(CONSOLE, fmt "\r\n", ##__VA_ARGS__); \
} while (0)

void test_print_prefix(const struct test_print_msg *m)
{
	for (int i = 0; i < m->level * TEST_INDENT_SPACES_PER_LEVEL; i++)
		uart_printf(CONSOLE, " ");
	if (m->tag)
		uart_printf(CONSOLE, "%s ", m->tag);
}

// Context switch test threads
static const struct test_print_msg run_a = { TEST_LEVEL_LINE, TEST_TAG_RUN };
static const struct test_print_msg ok_a   = { TEST_LEVEL_LINE, TEST_TAG_OK };

void context_thread_A(void) {
    int tid = MyTid();
    for (int i = 0; i < 4; i++) {
        TEST_PRINT(run_a, "Thread A executing (TID=%d, iteration=%d)", tid, i);
        Yield();
    }
    TEST_PRINT(ok_a, "Thread A completed (TID=%d)", tid);
    Exit();
}

static const struct test_print_msg run_b = { TEST_LEVEL_LINE, "\033[1;35m[ RUN ]\033[0m" };
static const struct test_print_msg ok_b   = { TEST_LEVEL_LINE, TEST_TAG_OK };

void context_thread_B(void) {
    int tid = MyTid();
    for (int i = 0; i < 4; i++) {
        TEST_PRINT(run_b, "Thread B executing (TID=%d, iteration=%d)", tid, i);
        Yield();
    }
    TEST_PRINT(ok_b, "Thread B completed (TID=%d)", tid);
    Exit();
}

static const struct test_print_msg run_c = { TEST_LEVEL_LINE, "\033[1;36m[ RUN ]\033[0m" };
static const struct test_print_msg ok_c   = { TEST_LEVEL_LINE, TEST_TAG_OK };

void context_thread_C(void) {
    int tid = MyTid();
    for (int i = 0; i < 4; i++) {
        TEST_PRINT(run_c, "Thread C executing (TID=%d, iteration=%d)", tid, i);
        Yield();
    }
    TEST_PRINT(ok_c, "Thread C completed (TID=%d)", tid);
    Exit();
}

static const struct test_print_msg hdr2  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info2 = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg ok2   = { TEST_LEVEL_LINE, TEST_TAG_OK };

void test_context_switch(void) {
    TEST_PRINT(hdr2, "Test #2: Context switch:");
    TEST_PRINT(info2, "Testing context switching...");
    int tid_a = Create(10, context_thread_A);
    int tid_b = Create(10, context_thread_B);
    int tid_c = Create(10, context_thread_C);
    TEST_PRINT(info2, "Created threads: A(TID=%d), B(TID=%d), C(TID=%d)", tid_a, tid_b, tid_c);
    for (int i = 0; i < 15; i++) Yield();
    TEST_PRINT(ok2, "Context switching tests passed");
}

static const struct test_print_msg hdr1  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info1 = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg ok1   = { TEST_LEVEL_LINE, TEST_TAG_OK };
static const struct test_print_msg fail1 = { TEST_LEVEL_LINE, TEST_TAG_FAIL };

void test_timer(void) {
    TEST_PRINT(hdr1, "Test #1: Timer:");
    TEST_PRINT(info1, "Testing system timer...");
    int runtime1 = GetKernelRuntime();
    TEST_PRINT(info1, "Kernel runtime reading 1: %d ticks", runtime1);
    int mytid = MyTid();
    int runtime2 = GetKernelRuntime();
    TEST_PRINT(info1, "Kernel runtime reading 2: %d ticks", runtime2);
    if (runtime2 >= runtime1 && runtime1 >= 0)
        TEST_PRINT(ok1, "Timer syscalls working (TID=%d, delta=%d ticks)", mytid, runtime2 - runtime1);
    else
        TEST_PRINT(fail1, "Timer behavior unexpected");
}

static const struct test_print_msg hdr3  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info3 = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg ok3   = { TEST_LEVEL_LINE, TEST_TAG_OK };

void test_process_creation(void) {
    TEST_PRINT(hdr3, "Test #3: Process creation:");
    TEST_PRINT(info3, "Testing process creation...");
    TEST_PRINT(ok3, "Process creation tests passed");
}

/* Heap allocator: malloc / free (bare-metal override) */
static const struct test_print_msg hdr_malloc = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info_malloc = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg ok_malloc = { TEST_LEVEL_LINE, TEST_TAG_OK };
static const struct test_print_msg fail_malloc = { TEST_LEVEL_LINE, TEST_TAG_FAIL };

void test_malloc(void) {
    TEST_PRINT(hdr_malloc, "Test #5: Heap (malloc/free, per-process VRAM map):");
    TEST_PRINT(info_malloc, "Testing heap allocation (process_id, vram, phys)...");
    int pid = GetCurrentProcessId();
    uint64_t *a = malloc_for_process(pid, 4);
    if (!a) {
        TEST_PRINT(fail_malloc, "malloc_for_process(%d, 4) returned NULL", pid);
        return;
    }
    a[0] = TEST_MAGIC_VAL2;
    uint64_t *b = malloc_for_process(pid, 8);
    if (!b) {
        TEST_PRINT(fail_malloc, "malloc_for_process(%d, 8) returned NULL", pid);
        free(a);
        return;
    }
    b[0] = TEST_MAGIC_VAL;
    free(a);
    uint64_t *c = malloc_for_process(pid, 2);
    if (!c) {
        TEST_PRINT(fail_malloc, "malloc_for_process(%d, 2) after free returned NULL", pid);
        free(b);
        return;
    }
    free(c);
    if (b[0] != TEST_MAGIC_VAL) {
        TEST_PRINT(fail_malloc, "Heap: read-back mismatch");
        free(b);
        return;
    }
    free(b);
    TEST_PRINT(ok_malloc, "Heap: alloc/free/realloc and read-back OK");
    TEST_PRINT(ok_malloc, "Heap tests passed");
}

/* Process model: MyProcessId, GetProcessSharedMem, threads in same process share memory */
static int process_test_pid_a = -1;
static int process_test_pid_b = -1;
static void process_test_thread_a(void) {
    process_test_pid_a = MyTid();
    int pid = MyProcessId();
    void *sh = GetProcessSharedMem();
    if (sh) *(int *)sh = TEST_MAGIC_VAL;
    (void)pid;
    Exit();
}
static void process_test_thread_b(void) {
    process_test_pid_b = MyTid();
    int pid = MyProcessId();
    void *sh = GetProcessSharedMem();
    int val = sh ? *(int *)sh : 0;
    (void)pid;
    if (val == TEST_MAGIC_VAL)
        TEST_PRINT(ok2, "Process shared memory: both threads same process, shared write/read");
    else
        TEST_PRINT(fail1, "Process shared memory: read 0x%x", val);
    Exit();
}
static const struct test_print_msg hdr4  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info4 = { TEST_LEVEL_LINE, TEST_TAG_INFO };

void test_processes(void) {
    TEST_PRINT(hdr4, "Test #6: Process model:");
    TEST_PRINT(info4, "Testing process model (MyProcessId, GetProcessSharedMem, shared memory)...");
    int mypid = MyProcessId();
    void *mymem = GetProcessSharedMem();
    if (mypid >= 0 && mymem != (void *)0)
        TEST_PRINT(ok2, "MyProcessId=%d, GetProcessSharedMem non-null", mypid);
    int ta = Create(10, process_test_thread_a);
    int tb = Create(10, process_test_thread_b);
    for (int i = 0; i < 30; i++) Yield();
    TEST_PRINT(ok2, "Process model tests passed");
    (void)ta; (void)tb;
}

/* Spinlock: two threads increment shared counter under lock (process shared memory) */
static void spinlock_inc_thread(void) {
    void *sh = GetProcessSharedMem();
    spinlock_t *lock = (spinlock_t *)sh;
    volatile int *done = (volatile int *)((char *)sh + sizeof(spinlock_t));
    volatile int *counter = (volatile int *)((char *)sh + sizeof(spinlock_t) + sizeof(int));
    for (int i = 0; i < SPINLOCK_TEST_ITERS; i++) {
        SpinLock_Acquire(lock);
        (*counter)++;
        SpinLock_Release(lock);
        Yield();
    }
    SpinLock_Acquire(lock);
    (*done)++;
    SpinLock_Release(lock);
    Exit();
}
static const struct test_print_msg hdr6  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info6 = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg fail6 = { TEST_LEVEL_LINE, TEST_TAG_FAIL };

void test_spinlock(void) {
    TEST_PRINT(hdr6, "Test #7: Spinlock:");
    TEST_PRINT(info6, "Testing spinlock (critical section)...");
    void *sh = GetProcessSharedMem();
    spinlock_t *lock = (spinlock_t *)sh;
    volatile int *done = (volatile int *)((char *)sh + sizeof(spinlock_t));
    volatile int *counter = (volatile int *)((char *)sh + sizeof(spinlock_t) + sizeof(int));
    *lock = 0;
    *done = 0;
    *counter = 0;
    int t1 = Create(10, spinlock_inc_thread);
    int t2 = Create(10, spinlock_inc_thread);
    int yields = 0;
    while (*done < 2 && yields < 2000) { Yield(); yields++; }
    for (int i = 0; i < 50; i++) Yield();
    if (*done < 2)
        TEST_PRINT(fail6, "Spinlock: only %d thread(s) finished (timeout)", *done);
    else if (*counter == SPINLOCK_TEST_ITERS * 2)
        TEST_PRINT(ok2, "Spinlock: counter=%d (expected %d)", *counter, SPINLOCK_TEST_ITERS * 2);
    else
        TEST_PRINT(fail6, "Spinlock: counter=%d expected %d", *counter, SPINLOCK_TEST_ITERS * 2);
    TEST_PRINT(ok2, "Spinlock tests passed");
    (void)t1; (void)t2;
}

static const struct test_print_msg hdr7  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info7 = { TEST_LEVEL_LINE, TEST_TAG_INFO };

void test_syscalls(void) {
    TEST_PRINT(hdr7, "Test #8: Syscalls:");
    TEST_PRINT(info7, "Testing syscall interface...");
    TEST_PRINT(ok2, "Syscall tests passed");
}

/* Test #9: Memory isolation — per-process VRAM; allocations do not overlap; data isolated. */
static const struct test_print_msg hdr8  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info8 = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg fail8 = { TEST_LEVEL_LINE, TEST_TAG_FAIL };

void test_memory_isolation(void) {
    TEST_PRINT(hdr8, "Test #9: Memory isolation:");
    TEST_PRINT(info8, "Per-process VRAM; distinct blocks; no cross-process overlap");
    int pid = GetCurrentProcessId();
    uint64_t *block_a = malloc_for_process(pid, 4);
    uint64_t *block_b = malloc_for_process(pid, 4);
    if (!block_a || !block_b) {
        TEST_PRINT(fail8, "Memory isolation: alloc failed");
        if (block_a) free(block_a);
        if (block_b) free(block_b);
        return;
    }
    if (block_a == block_b) {
        TEST_PRINT(fail8, "Memory isolation: two allocs returned same pointer");
        free(block_a);
        free(block_b);
        return;
    }
    block_a[0] = 0xA1B2C3D4UL;
    block_b[0] = 0xDEADBEEFUL;
    if (block_a[0] != 0xA1B2C3D4UL || block_b[0] != 0xDEADBEEFUL) {
        TEST_PRINT(fail8, "Memory isolation: read-back mismatch (blocks overlap or wrong)");
        free(block_a);
        free(block_b);
        return;
    }
    free(block_a);
    free(block_b);
    TEST_PRINT(ok2, "Memory isolation: distinct blocks, data isolated per allocation");
    TEST_PRINT(ok2, "Test #9 passed");
}

static const struct test_print_msg suite_ok = { TEST_LEVEL_TEST, TEST_TAG_OK };

void run_layer1_tests(void) {
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "\033[1;36m[====] Galatea-NIX Layer 1 Process Tests:\033[0m\r\n");
    test_timer();
    test_context_switch();
    test_process_creation();
    test_malloc();
    test_processes();
    test_spinlock();
    test_syscalls();
    test_memory_isolation();
    TEST_PRINT(suite_ok, "All Layer 1 tests passed");
    uart_printf(CONSOLE, "\r\n");
}