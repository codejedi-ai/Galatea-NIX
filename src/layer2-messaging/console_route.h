#ifndef CONSOLE_ROUTE_H
#define CONSOLE_ROUTE_H

/*
 * Optional sink for CONSOLE output. When active, uart_printf(CONSOLE, …) is
 * forwarded here instead of hitting the UART hardware directly. The display
 * server registers the sink so it alone owns console writes.
 */
typedef void (*console_sink_fn)(const char *data, int len);

void console_route_set(console_sink_fn fn);
int  console_route_active(void);
void console_route_bypass(int on);
void console_route_write(const char *data, int len);

#endif
