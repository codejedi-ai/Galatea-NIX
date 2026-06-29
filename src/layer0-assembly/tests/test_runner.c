#include <stdio.h>
#include "layer0_tests.h"
#include "../layer1-processes/tests/layer1_tests.h"
#include "../layer2-messaging/tests/layer2_tests.h"
#include "../layer3-services/tests/layer3_tests.h"
#include "../layer3-services/tests/display_tests.h"
#include "../../layer1-processes/project.h"

void run_layer4_tests(void);   /* real definition in layer3-services/tests/layer4_tests.c */

int main() {
    printf("Starting %s Test Suite...\n", PROJECT_DISPLAY_NAME);

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

    printf("\n=== Display Server Tests ===\n");
    run_display_server_tests();

    printf("\nAll tests completed!\n");
    return 0;
}