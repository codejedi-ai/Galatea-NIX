#include "util.h"
#include "rpi.h"

/* ascii digit to integer */
int a2d(char ch) {
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}

/* unsigned int to ascii string */
void ui2a(unsigned int num, unsigned int base, char *bf) {
	int n = 0;
	int dgt;
	unsigned int d = 1;

	while ((num / d) >= base) d *= base;
	while (d != 0) {
		dgt = num / d;
		num %= d;
		d /= base;
		if (n || dgt > 0 || d == 0) {
			*bf++ = dgt + (dgt < 10 ? '0' : 'a' - 10);
			++n;
		}
	}
	*bf = 0;
}

/* signed int to ascii string */
void i2a(int num, char *bf) {
	if (num < 0) {
		num = -num;
		*bf++ = '-';
	}
	ui2a(num, 10, bf);
}

/* Custom memset/memcpy to avoid SIMD instructions from compiler */
void *memset(void *s, int c, size_t n) {
	char *it = (char *)s;
	for (; n > 0; --n) *it++ = (char)c;
	return s;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
	char *sit = (char *)src;
	char *cdest = (char *)dest;
	for (size_t i = 0; i < n; ++i) cdest[i] = sit[i];
	return dest;
}
