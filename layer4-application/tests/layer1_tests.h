#ifndef LAYER1_TESTS_H
#define LAYER1_TESTS_H

#include "test_framework.h"

// Kernel layer tests
test_result_t test_process_creation(void);
test_result_t test_context_switch(void);
test_result_t test_scheduler(void);
test_result_t test_system_calls(void);
test_result_t test_memory_management(void);

// Kernel layer test suite
extern test_suite_t layer1_test_suite;

#endif // LAYER1_TESTS_H