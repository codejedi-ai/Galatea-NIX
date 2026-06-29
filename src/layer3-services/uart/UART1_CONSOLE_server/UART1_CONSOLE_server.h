#ifndef UART1_CONSOLE_SERVER_H
#define UART1_CONSOLE_SERVER_H

#include "../../layer1-processes/rpi.h"

#define IO_GETC 32
#define IO_PUTC 33
#define IO_CONSOLE_OBSERVE 35
#define IO_CONSOLE_UNOBSERVE 36
#define IO_CONSOLE_NOTIFY 37
#define IO_PEEK 38

#define CONSOLE_OBSERVE_RX 1
#define CONSOLE_OBSERVE_TX 2

void UART1_CONSOLE_server(void);

int ConsoleServerTid(void);
int ConsoleGetc(int server_tid);
int ConsolePoll(int server_tid);
int ConsolePutc(int server_tid, unsigned char ch);
int ConsoleObserve(int server_tid, uint8_t mask);
int ConsoleUnobserve(int server_tid);

#endif
