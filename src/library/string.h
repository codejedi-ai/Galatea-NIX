#ifndef _LIBRARY_STRING_H_
#define _LIBRARY_STRING_H_ 1

#include <stdint.h>

// String utility functions for the kernel
// These are available for future use if needed

// Convert ASCII character to digit
static inline int a2d(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return 0;
}

// Check if string is empty
static inline int8_t is_empty(const char *str) {
    return (*str == '\0');
}

// Check if string starts with "0x" (hex prefix)
static inline int8_t is_hex(const char *str) {
    if (str[0] == '\0') return 0;
    if (str[1] == '\0') return 0;
    return (str[0] == '0' && str[1] == 'x');
}

// Convert string to 64-bit integer (supports negative numbers)
int64_t atoi_64(const char *str);

// Convert hex string (with 0x prefix) to integer
int64_t str_to_hex(const char *str);

// Compare two strings for equality (returns non-zero if equal)
int strcmp_ret(const char *s1, const char *s2);

void *memmove(void *dst, const void *src, unsigned long n);
char *strncpy(char *dst, const char *src, unsigned long n);
int strncmp(const char *s1, const char *s2, unsigned long n);

#endif
