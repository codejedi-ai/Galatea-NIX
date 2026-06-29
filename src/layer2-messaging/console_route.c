#include "console_route.h"

static console_sink_fn g_sink;
static int g_bypass;

void console_route_set(console_sink_fn fn)
{
	g_sink = fn;
}

int console_route_active(void)
{
	return g_sink != 0 && !g_bypass;
}

void console_route_bypass(int on)
{
	g_bypass = on ? 1 : 0;
}

void console_route_write(const char *data, int len)
{
	if (g_sink && data && len > 0)
		g_sink(data, len);
}
