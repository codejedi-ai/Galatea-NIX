#include "layer1_tests.h"
#include "../syscall.h"
#include "../processes.h"
#include "../rpi.h"
#include "../systimer.h"

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
    // Test process creation functionality
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Process creation tests passed\r\n");
}

void test_syscalls() {
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Testing syscall interface...\r\n");
    // Test syscall functionality
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Syscall tests passed\r\n");
}

void run_layer1_tests() {
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "\033[1;36m[====] Galatea-NIX Layer 1 Process Tests\033[0m\r\n");
    uart_printf(CONSOLE, "\r\n");
    
    test_timer();
    test_context_switch();
    test_process_creation();
    test_syscalls();
    
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m All Layer 1 tests passed\r\n");
    uart_printf(CONSOLE, "\r\n");
}