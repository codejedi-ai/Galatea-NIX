#ifndef _LIBRARY_MATH_H_
#define _LIBRARY_MATH_H_ 1

#include <stdint.h>

// Mathematical utility functions for the kernel

// Return the minimum of two unsigned 64-bit integers
uint64_t min_u64(uint64_t a, uint64_t b);

// Return the maximum of two unsigned 64-bit integers
uint64_t max_u64(uint64_t a, uint64_t b);

// Return the minimum of two signed integers
int min_int(int a, int b);

// Return the maximum of two signed integers
int max_int(int a, int b);

#endif
