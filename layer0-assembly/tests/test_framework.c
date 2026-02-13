#include "test_framework.h"
#include <stdio.h>

void run_test(const char* test_name, void (*test_func)()) {
    printf("Running test: %s...", test_name);
    test_func();
    printf(" PASSED\n");
}

void run_all_tests(test_case* tests, int count) {
    for (int i = 0; i < count; i++) {
        run_test(tests[i].name, tests[i].test_func);
    }
}