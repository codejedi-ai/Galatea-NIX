#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>
#include "../shell.h"  // For UART output

// Test result structure
typedef enum {
    TEST_PASS,
    TEST_FAIL,
    TEST_SKIP
} test_result_t;

// Test function pointer type
typedef test_result_t (*test_func_t)(void);

// Individual test structure
typedef struct {
    const char* name;
    test_func_t func;
    int enabled;
} test_case_t;

// Test suite structure
typedef struct {
    const char* name;
    test_case_t* tests;
    int num_tests;
} test_suite_t;

// Test framework functions
void run_test(const char* test_name, test_func_t test_func);
void run_test_suite(test_suite_t* suite);
void print_test_summary(int total_tests, int passed_tests, int failed_tests);

// Macros for test assertions
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            uart_printf(CONSOLE, "[FAIL] %s:%d - %s\n", __FILE__, __LINE__, message); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            uart_printf(CONSOLE, "[FAIL] %s:%d - %s (Expected: %d, Actual: %d)\n", \
                        __FILE__, __LINE__, message, (int)(expected), (int)(actual)); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_STR_EQUAL(expected, actual, message) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            uart_printf(CONSOLE, "[FAIL] %s:%d - %s (Expected: %s, Actual: %s)\n", \
                        __FILE__, __LINE__, message, (expected), (actual)); \
            return TEST_FAIL; \
        } \
    } while(0)

#endif // TEST_FRAMEWORK_H