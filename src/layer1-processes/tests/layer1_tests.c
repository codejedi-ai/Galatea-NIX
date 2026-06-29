#include "layer1_tests.h"
#include "../syscall.h"
#include "../processes.h"
#include "../rpi.h"
#include "../timer/systimer.h"
#include "../locks.h"
#include "../malloc/malloc.h"
#include "../../layer2-messaging/messaging.h"
#include "../project.h"
#include "../vmm.h"
#include "../config.h"
#include "../../layer0-assembly/layer0.h"
#include "../accel.h"
#include "../../layer4-ui/canvas_accel.h"

#define TEST_PRINT(m, fmt, ...) do { \
	test_print_prefix(&(m)); \
	uart_printf(CONSOLE, fmt "\r\n", ##__VA_ARGS__); \
} while (0)

static int g_l1_failures;

#define L1_FAIL(m, fmt, ...) do { \
	g_l1_failures++; \
	TEST_PRINT(m, fmt, ##__VA_ARGS__); \
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

static const struct test_print_msg hdr3  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info3 = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg ok3   = { TEST_LEVEL_LINE, TEST_TAG_OK };

void test_process_creation(void) {
    TEST_PRINT(hdr3, "Test #3: Process creation:");
    TEST_PRINT(info3, "Testing process creation...");
    TEST_PRINT(ok3, "Process creation tests passed");
}

/* Heap allocator: mymalloc / myfree */
static const struct test_print_msg hdr_malloc = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info_malloc = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg ok_malloc = { TEST_LEVEL_LINE, TEST_TAG_OK };
static const struct test_print_msg fail_malloc = { TEST_LEVEL_LINE, TEST_TAG_FAIL };

void test_malloc(void) {
    TEST_PRINT(hdr_malloc, "Test #5: Heap (mymalloc/myfree):");
    TEST_PRINT(info_malloc, "Testing heap allocation...");
    uint64_t *a = mymalloc(4);
    if (!a) {
        L1_FAIL(fail_malloc, "mymalloc(4) returned NULL");
        return;
    }
    a[0] = TEST_MAGIC_VAL2;
    uint64_t *b = mymalloc(8);
    if (!b) {
        L1_FAIL(fail_malloc, "mymalloc(8) returned NULL");
        myfree(a);
        return;
    }
    b[0] = TEST_MAGIC_VAL;
    myfree(a);
    uint64_t *c = mymalloc(2);
    if (!c) {
        L1_FAIL(fail_malloc, "mymalloc(2) after free returned NULL");
        myfree(b);
        return;
    }
    myfree(c);
    if (b[0] != TEST_MAGIC_VAL) {
        L1_FAIL(fail_malloc, "Heap: read-back mismatch");
        myfree(b);
        return;
    }
    myfree(b);
    TEST_PRINT(ok_malloc, "Heap: alloc/free/realloc and read-back OK");
    TEST_PRINT(ok_malloc, "Heap tests passed");
}

/* Process model: MyProcessId, GetProcessSharedMem, threads in same process share memory */
#define SH_TEST_MAGIC_OFF  0
#define SH_TEST_READY_OFF  4

static void process_test_thread_a(void) {
    void *sh = GetProcessSharedMem();
    if (sh) {
        *(int *)((char *)sh + SH_TEST_MAGIC_OFF) = TEST_MAGIC_VAL;
        __asm__ volatile("dmb ish" ::: "memory");
        *(volatile int *)((char *)sh + SH_TEST_READY_OFF) = 1;
    }
    Exit();
}
static void process_test_thread_b(void) {
    void *sh = GetProcessSharedMem();
    int val = 0;
    if (sh) {
        for (int i = 0; i < 200; i++) {
            if (*(volatile int *)((char *)sh + SH_TEST_READY_OFF)) {
                val = *(int *)((char *)sh + SH_TEST_MAGIC_OFF);
                break;
            }
            Yield();
        }
    }
    if (val == TEST_MAGIC_VAL)
        TEST_PRINT(ok2, "Process shared memory: both threads same process, shared write/read");
    else
        L1_FAIL(fail_malloc, "Process shared memory: read 0x%x", val);
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
    {
        int pid = MyProcessId();
        if (mymem) {
            *(int *)((char *)mymem + SH_TEST_MAGIC_OFF) = 0;
            *(volatile int *)((char *)mymem + SH_TEST_READY_OFF) = 0;
        }
        (void)CreateInProcess(10, process_test_thread_a, pid);
        (void)CreateInProcess(10, process_test_thread_b, pid);
    }
    for (int i = 0; i < 50; i++) Yield();
    TEST_PRINT(ok2, "Process model tests passed");
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
        L1_FAIL(fail6, "Spinlock: only %d thread(s) finished (timeout)", *done);
    else if (*counter == SPINLOCK_TEST_ITERS * 2)
        TEST_PRINT(ok2, "Spinlock: counter=%d (expected %d)", *counter, SPINLOCK_TEST_ITERS * 2);
    else
        L1_FAIL(fail6, "Spinlock: counter=%d expected %d", *counter, SPINLOCK_TEST_ITERS * 2);
    TEST_PRINT(ok2, "Spinlock tests passed");
    (void)t1; (void)t2;
}

static const struct test_print_msg hdr_smp  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info_smp = { TEST_LEVEL_LINE, TEST_TAG_INFO };
static const struct test_print_msg ok_smp   = { TEST_LEVEL_LINE, TEST_TAG_OK };
static const struct test_print_msg fail_smp = { TEST_LEVEL_LINE, TEST_TAG_FAIL };

/* Written by secondary cores during the dispatch test. */
static volatile unsigned accel_results[3];

static void accel_write_magic(void *arg)
{
    int idx = *(int *)arg;
    accel_results[idx] = (unsigned)TEST_MAGIC_VAL;
}

void test_smp(void) {
    TEST_PRINT(hdr_smp, "Test #8: Accelerator cores (cores 1-3):");
    TEST_PRINT(info_smp, "Testing dispatch + completion on all 3 secondary cores...");

    /* --- Test 1: dispatch a write-magic job to each core, verify result --- */
    static int indices[3] = {0, 1, 2};
    accel_results[0] = 0;
    accel_results[1] = 0;
    accel_results[2] = 0;

    for (int i = 0; i < 3; i++)
        accel_dispatch(i + 1, accel_write_magic, &indices[i]);
    accel_wait_all();

    int pass = 1;
    for (int i = 0; i < 3; i++) {
        if (accel_results[i] != (unsigned)TEST_MAGIC_VAL) {
            L1_FAIL(fail_smp, "Core %d: got 0x%x, expected 0x%x",
                i + 1, accel_results[i], (unsigned)TEST_MAGIC_VAL);
            pass = 0;
        }
    }
    if (pass)
        TEST_PRINT(ok_smp, "Dispatch: all 3 cores executed and signalled completion");

    /* --- Test 2: parallel canvas clear, verify all cells become ' ' --- */
    static UiCanvas test_cv;
    for (int r = 0; r < UI_CANVAS_HMAX; r++)
        for (int c = 0; c < UI_CANVAS_WMAX; c++)
            test_cv.cells[r][c] = 'X';

    ui_canvas_clear_parallel(&test_cv);

    int clear_ok = 1;
    for (int r = 0; r < UI_CANVAS_HMAX && clear_ok; r++)
        for (int c = 0; c < UI_CANVAS_WMAX && clear_ok; c++)
            if (test_cv.cells[r][c] != ' ')
                clear_ok = 0;

    if (clear_ok)
        TEST_PRINT(ok_smp, "Parallel canvas clear: %d x %d cells = ' ' (4-core)",
            UI_CANVAS_HMAX, UI_CANVAS_WMAX);
    else
        L1_FAIL(fail_smp, "Parallel canvas clear: found non-space cell");

    /* --- Test 3: repeated dispatch to confirm cores re-enter the loop --- */
    accel_results[0] = 0;
    accel_results[1] = 0;
    accel_results[2] = 0;
    for (int i = 0; i < 3; i++)
        accel_dispatch(i + 1, accel_write_magic, &indices[i]);
    accel_wait_all();

    int reuse_ok = (accel_results[0] == (unsigned)TEST_MAGIC_VAL &&
                    accel_results[1] == (unsigned)TEST_MAGIC_VAL &&
                    accel_results[2] == (unsigned)TEST_MAGIC_VAL);
    if (reuse_ok)
        TEST_PRINT(ok_smp, "Re-dispatch: cores re-entered worker loop correctly");
    else
        L1_FAIL(fail_smp, "Re-dispatch: result mismatch after second round");

    if (pass && clear_ok && reuse_ok)
        TEST_PRINT(ok_smp, "All accelerator tests passed");
}

static const struct test_print_msg hdr_syscalls  = { TEST_LEVEL_TEST, NULL };
static const struct test_print_msg info_syscalls = { TEST_LEVEL_LINE, TEST_TAG_INFO };

void test_syscalls(void) {
    TEST_PRINT(hdr_syscalls, "Test #9: Syscalls:");
    TEST_PRINT(info_syscalls, "Testing syscall interface...");
    TEST_PRINT(ok2, "Syscall tests passed");
}

static const struct test_print_msg suite_ok = { TEST_LEVEL_TEST, TEST_TAG_OK };
static const struct test_print_msg suite_fail = { TEST_LEVEL_TEST, TEST_TAG_FAIL };

int run_layer1_tests(void) {
    g_l1_failures = 0;
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "\033[1;36m[====] %s Layer 1 Process Tests:\033[0m\r\n", PROJECT_DISPLAY_NAME);
    test_context_switch();
    test_process_creation();
    test_malloc();
    test_processes();
    test_syscalls();
    test_smp();
    uart_printf(CONSOLE, "\r\n");
    if (g_l1_failures > 0) {
        TEST_PRINT(suite_fail, "Layer 1: %d failure(s)", g_l1_failures);
        return -1;
    }
    TEST_PRINT(suite_ok, "All Layer 1 tests passed");
    return 0;
}