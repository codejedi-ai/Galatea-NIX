#ifndef LAYER3_TESTS_H
#define LAYER3_TESTS_H

#include "test_framework.h"

// Application layer tests
test_result_t test_train_control(void);
test_result_t test_ui_interface(void);
test_result_t test_track_management(void);
test_result_t test_communication_protocol(void);

// Application layer test suite
extern test_suite_t layer3_test_suite;

#endif // LAYER3_TESTS_H