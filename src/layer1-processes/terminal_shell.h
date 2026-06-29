#ifndef TERMINAL_SHELL_H
#define TERMINAL_SHELL_H 1

/*
 * UART-only interactive shell for DarcyOS APU — no display server, no UI layer.
 * Created from kmain when START_SHELL is enabled.
 */
void terminal_shell_entry(void);

#endif
