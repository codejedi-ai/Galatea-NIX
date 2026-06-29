#include "math.h"

uint64_t min_u64(uint64_t a, uint64_t b) {
    return (a < b) ? a : b;
}

uint64_t max_u64(uint64_t a, uint64_t b) {
    return (a > b) ? a : b;
}

int min_int(int a, int b) {
    return (a < b) ? a : b;
}

int max_int(int a, int b) {
    return (a > b) ? a : b;
}
