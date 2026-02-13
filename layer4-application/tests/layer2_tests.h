#ifndef LAYER2_TESTS_H
#define LAYER2_TESTS_H

#include "test_framework.h"

// Services layer tests
test_result_t test_clock_server(void);
test_result_t test_io_server(void);
test_result_t test_name_server(void);
test_result_t test_game_server(void);

// Services layer test suite
extern test_suite_t layer2_test_suite;

#endif // LAYER2_TESTS_H