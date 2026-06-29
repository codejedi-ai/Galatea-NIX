#ifndef IO_API_H
#define IO_API_H

#include "../../layer1-processes/rpi.h"

#define UART1_CONSOLE_SERVER "UART1_CONSOLE_server"
#define UART2_MARKLIN_SERVER "UART2_MARKLIN_server"

void UART1_CONSOLE_server(void);
void UART2_MARKLIN_server(void);
void io_notifier(void);

int MarklinServerTid(void);
int Getc(int tid, int channel);
int Putc(int tid, int channel, unsigned char ch);
int Put2c(int tid, int channel, unsigned char ch, unsigned char ch2);
int awaitCTS(int tid, int channel, uint8_t val);

#endif
