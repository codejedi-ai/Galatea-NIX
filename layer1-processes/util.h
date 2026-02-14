#ifndef _util_h_
#define _util_h_ 1

#include <stddef.h>

int a2d(char ch);
void ui2a(unsigned int num, unsigned int base, char *bf);
void i2a(int num, char *bf);

void *memset(void *s, int c, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);

#endif
