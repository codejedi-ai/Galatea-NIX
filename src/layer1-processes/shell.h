#ifndef _shell_h_
#define _shell_h_ 1

/*
 * Interactive serial shell (an in-kernel "terminal" task).
 * Reads command lines from the console UART and runs built-in commands.
 * Created from kmain after the boot test suite when START_SHELL is enabled.
 */
void shell(void);            /* the Terminal app; returns to the launcher on `home` */
void ui_boot_advisory(void); /* after boot: warn if terminal is too small */
void ui_boot_entry(void);  /* boot advisory, then launcher */

#endif /* shell.h */
