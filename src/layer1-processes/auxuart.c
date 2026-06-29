#include "auxuart.h"
#include "mmio_config.h"
#include "config.h"
#include <stdint.h>

/* AUX mini-UART register offsets within the AUX block (BCM2711 datasheet;
 * matches QEMU bcm2835-aux). The block base comes from BOARD.aux_base. */
#define AUX_ENABLES   0x04   /* bit0 enables the mini-UART  */
#define AUX_MU_IO     0x40   /* RX/TX data                  */
#define AUX_MU_IER    0x44   /* interrupt enable            */
#define AUX_MU_IIR    0x48   /* interrupt id / FIFO clear   */
#define AUX_MU_LCR    0x4C   /* line control (data bits)    */
#define AUX_MU_MCR    0x50   /* modem control (RTS)         */
#define AUX_MU_LSR    0x54   /* line status                 */
#define AUX_MU_CNTL   0x60   /* extra control (TX/RX en)    */
#define AUX_MU_BAUD   0x68   /* baud-rate divisor           */

#define MU_LSR_RX_READY  0x01   /* receive FIFO has a byte */
#define MU_LSR_TX_EMPTY  0x20   /* transmit FIFO has space */

#define AUX_REG(off) MMIO_READ32(BOARD.aux_base + (off))
#define AUX_SET(off, v) MMIO_WRITE32(BOARD.aux_base + (off), (v))

void auxuart_init(void)
{
	AUX_SET(AUX_ENABLES, AUX_REG(AUX_ENABLES) | 1u); /* enable mini-UART */
	AUX_SET(AUX_MU_CNTL, 0);      /* TX/RX off while configuring */
	AUX_SET(AUX_MU_IER,  0);      /* no interrupts: the link server polls */
	AUX_SET(AUX_MU_LCR,  3);      /* 8-bit words */
	AUX_SET(AUX_MU_MCR,  0);      /* RTS asserted */
	AUX_SET(AUX_MU_BAUD, 270);    /* ~115200 @ 250MHz; QEMU ignores the rate */
	AUX_SET(AUX_MU_IIR,  0xC6);   /* clear & enable the 8-byte FIFOs */
	AUX_SET(AUX_MU_CNTL, 3);      /* enable transmitter + receiver */
}

void auxuart_putc(unsigned char c)
{
	while (!(AUX_REG(AUX_MU_LSR) & MU_LSR_TX_EMPTY)) { }
	AUX_SET(AUX_MU_IO, c);
}

int auxuart_getc_nb(void)
{
	if (AUX_REG(AUX_MU_LSR) & MU_LSR_RX_READY)
		return (int)(AUX_REG(AUX_MU_IO) & 0xFF);
	return -1;
}

unsigned char auxuart_getc(void)
{
	while (!(AUX_REG(AUX_MU_LSR) & MU_LSR_RX_READY)) { }
	return (unsigned char)(AUX_REG(AUX_MU_IO) & 0xFF);
}
