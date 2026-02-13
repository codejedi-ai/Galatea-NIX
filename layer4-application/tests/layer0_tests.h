#ifndef LAYER0_TESTS_H
#define LAYER0_TESTS_H

#include "test_framework.h"

// Assembly layer tests
test_result_t test_stack_operations(void);
test_result_t test_exception_handling(void);
test_result_t test_register_save_restore(void);

// Assembly layer test suite
extern test_suite_t layer0_test_suite;

#endif // LAYER0_TESTS_H