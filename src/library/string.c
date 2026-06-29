#include "string.h"

int64_t atoi_64(const char *str) {
    uint8_t is_neg = 0;
    if (str[0] == '-') {
        is_neg = 1;
        str++;
    }
    uint64_t ret = 0;
    while (*str != '\0') {
        ret = 10 * ret;
        ret += a2d(*str);
        str++;
    }
    return is_neg ? -(int64_t)ret : (int64_t)ret;
}

int64_t str_to_hex(const char *str) {
    uint64_t ret = 0;
    // Skip "0x" prefix
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }
    while (*str != '\0') {
        ret = 16 * ret;
        ret += a2d(*str);
        str++;
    }
    return ret;
}

int strcmp_ret(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) {
            return 0;
        }
        s1++;
        s2++;
    }
    return (*s1 == *s2);
}

void *memmove(void *dst, const void *src, unsigned long n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;

	if (d == s || n == 0)
		return dst;
	if (d < s) {
		for (unsigned long i = 0; i < n; i++)
			d[i] = s[i];
	} else {
		for (unsigned long i = n; i > 0; i--)
			d[i - 1] = s[i - 1];
	}
	return dst;
}

char *strncpy(char *dst, const char *src, unsigned long n)
{
	unsigned long i;

	for (i = 0; i + 1 < n && src[i] != '\0'; i++)
		dst[i] = src[i];
	if (n > 0)
		dst[i] = '\0';
	return dst;
}

int strncmp(const char *s1, const char *s2, unsigned long n)
{
	for (unsigned long i = 0; i < n; i++) {
		if (s1[i] != s2[i])
			return (unsigned char)s1[i] - (unsigned char)s2[i];
		if (s1[i] == '\0')
			return 0;
	}
	return 0;
}
