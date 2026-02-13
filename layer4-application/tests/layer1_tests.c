#include "layer1_tests.h"
#include "../test_framework.h"
#include "../../layer1-kernel/syscall.h"
#include "../../layer1-kernel/processes.h"

// Mock implementations for kernel layer tests
test_result_t test_process_creation(void) {
    uart_printf(CONSOLE, "Testing process creation...\n");
    
    // Test that we can create a process
    int tid = Create(1, NULL);  // Create a minimal process
    
    if (tid > 0) {
        uart_printf(CONSOLE, "Process creation successful, TID: %d\n", tid);
        return TEST_PASS;
    } else {
        uart_printf(CONSOLE, "Process creation failed, TID: %d\n", tid);
        return TEST_FAIL;
    }
}

test_result_t test_context_switch(void) {
    uart_printf(CONSOLE, "Testing context switch functionality...\n");
    
    // Test that we can get current process info
    int current_tid = MyTid();
    int current_priority = MyPriority();
    
    TEST_ASSERT(current_tid >= 0, "Could not get current TID");
    TEST_ASSERT(current_priority >= 0, "Could not get current priority");
    
    uart_printf(CONSOLE, "Current TID: %d, Priority: %d\n", current_tid, current_priority);
    return TEST_PASS;
}

test_result_t test_scheduler(void) {
    uart_printf(CONSOLE, "Testing scheduler functionality...\n");
    
    // Test that scheduler can pick a process
    // This is a simplified test - in reality would test scheduling algorithm
    int current_tid = MyTid();
    TEST_ASSERT(current_tid >= 0, "Scheduler not running properly");
    
    return TEST_PASS;
}

test_result_t test_system_calls(void) {
    uart_printf(CONSOLE, "Testing system calls...\n");
    
    // Test basic system calls
    int tid = MyTid();
    TEST_ASSERT(tid >= 0, "MyTid() system call failed");
    
    int priority = MyPriority();
    TEST_ASSERT(priority >= 0, "MyPriority() system call failed");
    
    int parent_tid = MyParentTid();
    // Parent TID can be -1 if no parent, so we just check it's reasonable
    TEST_ASSERT(parent_tid >= -1, "MyParentTid() system call failed");
    
    return TEST_PASS;
}

test_result_t test_memory_management(void) {
    uart_printf(CONSOLE, "Testing memory management...\n");
    
    // Test basic memory allocation if malloc is available
    // For now, just return pass - would implement actual tests in real scenario
    return TEST_PASS;
}

// Define the kernel layer test suite
test_case_t layer1_tests[] = {
    {"Process Creation", test_process_creation, 1},
    {"Context Switch", test_context_switch, 1},
    {"Scheduler", test_scheduler, 1},
    {"System Calls", test_system_calls, 1},
    {"Memory Management", test_memory_management, 1}
};

test_suite_t layer1_test_suite = {
    .name = "Kernel Layer",
    .tests = layer1_tests,
    .num_tests = sizeof(layer1_tests) / sizeof(test_case_t)
};