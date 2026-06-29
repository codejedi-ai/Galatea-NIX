#include "apps.h"

int app_uitoa(unsigned v, char *out)
{
	char t[16];
	int n = 0;
	if (v == 0) t[n++] = '0';
	while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
	for (int i = 0; i < n; i++) out[i] = t[n - 1 - i];
	out[n] = '\0';
	return n;
}

void app_append(char *dst, int *pos, const char *src)
{
	while (*src) dst[(*pos)++] = *src++;
	dst[*pos] = '\0';
}
