#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

typedef struct {
    const char* name;
    void (*test_func)();
} test_case;

void run_test(const char* test_name, void (*test_func)());
void run_all_tests(test_case* tests, int count);

#endif