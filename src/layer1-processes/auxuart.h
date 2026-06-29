#ifndef AUXUART_H
#define AUXUART_H

/*
 * BCM2835 AUX mini-UART driver — the OS's link to virtual hardware running in
 * the docker container (QEMU raspi4b serial1). This is the second UART, playing
 * the role of the CS452 Marklin train-control line: instead of a real Marklin
 * box it talks to a container-side simulator/gateway over a host chardev.
 *
 * It is a 16550-like device (NOT a PL011), so it does not share the rpi.c UART
 * code. Polled TX/RX; the link server's notifier drains it.
 */
void auxuart_init(void);              /* enable + 8-bit, polled, TX/RX on */
void auxuart_putc(unsigned char c);   /* blocking-on-TX-space write */
int  auxuart_getc_nb(void);           /* one byte, or -1 if RX FIFO empty */
unsigned char auxuart_getc(void);     /* blocking read (spins until a byte arrives) */

#endif
