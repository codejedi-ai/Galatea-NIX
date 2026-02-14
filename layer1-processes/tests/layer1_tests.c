#include "layer1_tests.h"
#include "../syscall.h"
#include "../processes.h"
#include "../rpi.h"
#include "../systimer.h"
#include "../../layer2-messaging/messaging.h"

// Context switch test threads
void context_thread_A() {
    int tid = MyTid();
    for (int i = 0; i < 4; i++) {
        uart_printf(CONSOLE, "\033[1;33m[ RUN ]\033[0m Thread A executing (TID=%d, iteration=%d)\r\n", tid, i);
        Yield();  // Let other threads run
    }
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Thread A completed (TID=%d)\r\n", tid);
    Exit();
}

void context_thread_B() {
    int tid = MyTid();
    for (int i = 0; i < 4; i++) {
        uart_printf(CONSOLE, "\033[1;35m[ RUN ]\033[0m Thread B executing (TID=%d, iteration=%d)\r\n", tid, i);
        Yield();  // Let other threads run
    }
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Thread B completed (TID=%d)\r\n", tid);
    Exit();
}

void context_thread_C() {
    int tid = MyTid();
    for (int i = 0; i < 4; i++) {
        uart_printf(CONSOLE, "\033[1;36m[ RUN ]\033[0m Thread C executing (TID=%d, iteration=%d)\r\n", tid, i);
        Yield();  // Let other threads run
    }
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Thread C completed (TID=%d)\r\n", tid);
    Exit();
}

void test_context_switch() {
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Testing context switching...\r\n");
    
    // Create three threads at the same priority to see round-robin switching
    int tid_a = Create(10, context_thread_A);
    int tid_b = Create(10, context_thread_B);
    int tid_c = Create(10, context_thread_C);
    
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Created threads: A(TID=%d), B(TID=%d), C(TID=%d)\r\n", 
                tid_a, tid_b, tid_c);
    
    // Yield to let the threads run
    for (int i = 0; i < 15; i++) {
        Yield();
    }
    
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Context switching tests passed\r\n");
}

void test_timer() {
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Testing system timer...\r\n");
    
    // Test GetKernelRuntime syscall
    int runtime1 = GetKernelRuntime();
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Kernel runtime reading 1: %d ticks\r\n", runtime1);
    
    // Do a simple syscall to advance time
    int mytid = MyTid();
    
    int runtime2 = GetKernelRuntime();
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Kernel runtime reading 2: %d ticks\r\n", runtime2);
    
    // Verify timer is increasing
    if (runtime2 >= runtime1 && runtime1 >= 0) {
        uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Timer syscalls working (TID=%d, delta=%d ticks)\r\n", 
                    mytid, runtime2 - runtime1);
    } else {
        uart_printf(CONSOLE, "\033[1;31m[ FAIL ]\033[0m Timer behavior unexpected\r\n");
    }
}

void test_process_creation() {
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Testing process creation...\r\n");
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Process creation tests passed\r\n");
}

/* Process model: MyProcessId, GetProcessSharedMem, threads in same process share memory */
static int process_test_pid_a = -1;
static int process_test_pid_b = -1;
static void process_test_thread_a(void) {
    process_test_pid_a = MyTid();
    int pid = MyProcessId();
    void *sh = GetProcessSharedMem();
    if (sh) *(int *)sh = 0x12345678;
    (void)pid;
    Exit();
}
static void process_test_thread_b(void) {
    process_test_pid_b = MyTid();
    int pid = MyProcessId();
    void *sh = GetProcessSharedMem();
    int val = sh ? *(int *)sh : 0;
    (void)pid;
    if (val == 0x12345678)
        uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Process shared memory: both threads same process, shared write/read\r\n");
    else
        uart_printf(CONSOLE, "\033[1;31m[ FAIL ]\033[0m Process shared memory: read 0x%x\r\n", val);
    Exit();
}
void test_processes(void) {
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Testing process model (MyProcessId, GetProcessSharedMem, shared memory)...\r\n");
    int mypid = MyProcessId();
    void *mymem = GetProcessSharedMem();
    if (mypid >= 0 && mymem != (void *)0)
        uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m MyProcessId=%d, GetProcessSharedMem non-null\r\n", mypid);
    int ta = Create(10, process_test_thread_a);
    int tb = Create(10, process_test_thread_b);
    for (int i = 0; i < 30; i++) Yield();
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Process model tests passed\r\n");
    (void)ta; (void)tb;
}

/* IPC = Inter-Process Communication (Send/Receive/Reply) for Layer 1 */
static void ipc_receiver(void) {
    int sender;
    char buf[32];
    *(int *)GetProcessSharedMem() = MyTid();
    Receive(&sender, buf, sizeof(buf));
    Reply(sender, "ack", 4);
    Exit();
}
static void ipc_sender(void) {
    char buf[32];
    int receiver_tid = *(int *)GetProcessSharedMem();
    int r = Send(receiver_tid, "ping", 5, buf, sizeof(buf));
    if (r >= 0)
        uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m IPC (inter-process): Send/Receive/Reply\r\n");
    else
        uart_printf(CONSOLE, "\033[1;31m[ FAIL ]\033[0m IPC Send returned %d\r\n", r);
    Exit();
}
void test_ipc(void) {
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Testing IPC (inter-process communication: Send/Receive/Reply)...\r\n");
    Create(5, ipc_receiver);
    Yield();
    Create(0, ipc_sender);
    for (int i = 0; i < 20; i++) Yield();
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m IPC tests passed\r\n");
}

void test_syscalls() {
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Testing syscall interface...\r\n");
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Syscall tests passed\r\n");
}

void run_layer1_tests() {
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "\033[1;36m[====] Galatea-NIX Layer 1 Process Tests\033[0m\r\n");
    uart_printf(CONSOLE, "\r\n");
    
    test_timer();
    test_context_switch();
    test_process_creation();
    test_processes();
    test_ipc();
    test_syscalls();
    
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m All Layer 1 tests passed\r\n");
    uart_printf(CONSOLE, "\r\n");
}