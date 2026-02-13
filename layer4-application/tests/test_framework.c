#include "test_framework.h"
#include <stdio.h>
#include <string.h>
#include "../../layer1-kernel/util.h"  // Assuming util functions exist
#include "../../layer1-kernel/rpi.h"   // For UART output

void run_test(const char* test_name, test_func_t test_func) {
    uart_printf(CONSOLE, "[RUN] %s...\n", test_name);
    
    test_result_t result = test_func();
    
    switch(result) {
        case TEST_PASS:
            uart_printf(CONSOLE, "[PASS] %s\n", test_name);
            break;
        case TEST_FAIL:
            uart_printf(CONSOLE, "[FAIL] %s\n", test_name);
            break;
        case TEST_SKIP:
            uart_printf(CONSOLE, "[SKIP] %s\n", test_name);
            break;
    }
}

void run_test_suite(test_suite_t* suite) {
    uart_printf(CONSOLE, "\n=== Running Test Suite: %s ===\n", suite->name);
    
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    
    for (int i = 0; i < suite->num_tests; i++) {
        if (suite->tests[i].enabled) {
            total_tests++;
            test_result_t result = suite->tests[i].func();
            
            if (result == TEST_PASS) {
                passed_tests++;
                uart_printf(CONSOLE, "[PASS] %s::%s\n", suite->name, suite->tests[i].name);
            } else if (result == TEST_FAIL) {
                failed_tests++;
                uart_printf(CONSOLE, "[FAIL] %s::%s\n", suite->name, suite->tests[i].name);
            } else { // TEST_SKIP
                uart_printf(CONSOLE, "[SKIP] %s::%s\n", suite->name, suite->tests[i].name);
            }
        }
    }
    
    uart_printf(CONSOLE, "\n=== %s Results: %d/%d tests passed ===\n\n", 
                suite->name, passed_tests, total_tests);
}

void print_test_summary(int total_tests, int passed_tests, int failed_tests) {
    uart_printf(CONSOLE, "\n=== FINAL TEST SUMMARY ===\n");
    uart_printf(CONSOLE, "Total Tests: %d\n", total_tests);
    uart_printf(CONSOLE, "Passed: %d\n", passed_tests);
    uart_printf(CONSOLE, "Failed: %d\n", failed_tests);
    uart_printf(CONSOLE, "Success Rate: %.2f%%\n", 
                total_tests > 0 ? (float)passed_tests / total_tests * 100.0f : 0.0f);
    uart_printf(CONSOLE, "=========================\n");
}