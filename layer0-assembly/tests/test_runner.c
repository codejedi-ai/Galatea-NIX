#include <stdio.h>
#include "layer0_tests.h"
#include "../layer1-processes/tests/layer1_tests.h"
#include "../layer2-messaging/tests/layer2_tests.h"

// Placeholder functions for missing layers
void run_layer3_tests() {}
void run_layer4_tests() {}

int main() {
    printf("Starting Galatea-NIX Test Suite...\n");

    printf("\n=== Layer 0 Tests ===\n");
    run_layer0_tests();

    printf("\n=== Layer 1 Tests ===\n");
    run_layer1_tests();

    printf("\n=== Layer 2 Tests ===\n");
    run_layer2_tests();

    printf("\n=== Layer 3 Tests ===\n");
    run_layer3_tests();

    printf("\n=== Layer 4 Tests ===\n");
    run_layer4_tests();

    printf("\nAll tests completed!\n");
    return 0;
}