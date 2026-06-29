#ifndef _rpi_h_
#define _rpi_h_ 1

#include <stdint.h>
#include <stddef.h>

// Serial line 1 on the RPi hat is used for the console
#define CONSOLE 1
#define MARKLIN 2

volatile uint32_t* get_RIS(uint32_t line);
uint32_t get_CTS(size_t line);
volatile uint32_t* get_ICR(size_t line);
void enable_RX_and_TX();


unsigned char uart_getc_modified(size_t line);
unsigned char uart_getc_queue(size_t line);
void uart_putc(size_t line, unsigned char c);
unsigned char uart_getc(size_t line);
void uart_putl(size_t line, const char *buf, size_t blen);
void uart_puts(size_t line, const char *buf);
void uart_printf(size_t line, char *fmt, ...);

void uart_init();
void uart_config_and_enable(size_t line, uint32_t baudrate);
void uart_config_and_enable_marklin();

int uart_rxc(size_t line);
int uart_cts(size_t line);

#include "timer/stimer.h"

#endif /* rpi.h */