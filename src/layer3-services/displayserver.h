#ifndef DISPLAYSERVER_H
#define DISPLAYSERVER_H

#define DISPLAY_SERVER_PRIORITY 4

/* Canonical screen dimensions (display-server task only). Clients use display_get_size(). */
extern int g_display_rows;
extern int g_display_cols;
extern int g_display_manual;

void display_server_entry(void);
int  DisplayServerTid(void);

/* Called from display server only — suppresses console routing during UART I/O. */
int  display_server_in_handler(void);

#endif
