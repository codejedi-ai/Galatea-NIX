#include "test_runner.h"
#include "test_framework.h"
#include "layer0_tests.h"
#include "layer1_tests.h"
#include "layer2_tests.h"
#include "layer3_tests.h"
#include "../shell.h"

void run_all_layer_tests(void) {
    uart_printf(CONSOLE, "\n=========================================\n");
    uart_printf(CONSOLE, "  GALATEA-NIX KERNEL LAYERED TEST SUITE  \n");
    uart_printf(CONSOLE, "=========================================\n");
    
    // Array of all test suites
    test_suite_t* all_suites[] = {
        &layer0_test_suite,  // Assembly layer
        &layer1_test_suite,  // Kernel layer
        &layer2_test_suite,  // Services layer
        &layer3_test_suite   // Application layer
    };
    
    int num_suites = sizeof(all_suites) / sizeof(test_suite_t*);
    
    int total_tests = 0;
    int total_passed = 0;
    int total_failed = 0;
    
    // Run each test suite
    for (int i = 0; i < num_suites; i++) {
        run_test_suite(all_suites[i]);
        
        // Count results for summary
        for (int j = 0; j < all_suites[i]->num_tests; j++) {
            if (all_suites[i]->tests[j].enabled) {
                total_tests++;
                
                test_result_t result = all_suites[i]->tests[j].func();
                if (result == TEST_PASS) {
                    total_passed++;
                } else if (result == TEST_FAIL) {
                    total_failed++;
                }
            }
        }
    }
    
    // Print final summary
    print_test_summary(total_tests, total_passed, total_failed);
    
    uart_printf(CONSOLE, "\n=========================================\n");
    uart_printf(CONSOLE, "  TEST SUITE COMPLETE                    \n");
    uart_printf(CONSOLE, "=========================================\n");
}