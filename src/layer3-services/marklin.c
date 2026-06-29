#include "marklin.h"
#include "nameserver.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/config.h"
#include "../layer1-processes/rpi.h"
#include "../layer1-processes/auxuart.h"

/*
 * Transport — selected at compile time via MARKLIN_HW_UART3:
 *
 *   0 (QEMU, default): AUX mini-UART (serial1).  marklin-virtual-hardware
 *             connects over TCP on serial1 and simulates Marklin 6051.
 *             No CTS polling — AUX UART has no hardware flow control.
 *
 *   1 (real Pi): PL011 UART3 @ 0xFE201600, 2400 baud → Marklin 6051 box.
 *             CTS is driven by the controller; each byte takes ~4.2 ms.
 *             Full 5-module poll (15 bytes) costs ~63 ms.
 *
 * Build with -DMARKLIN_HW_UART3=1 for real-Pi images.
 */

static void mk_putc(unsigned char c)
{
#if MARKLIN_HW_UART3
	while (!uart_cts(MARKLIN)) { }   /* wait for Marklin to de-assert CTS */
	uart_putc(MARKLIN, c);
#else
	auxuart_putc(c);
#endif
}

static unsigned char mk_getc(void)
{
#if MARKLIN_HW_UART3
	return uart_getc(MARKLIN);    /* blocking PL011 read */
#else
	return auxuart_getc();        /* blocking AUX mini-UART read */
#endif
}

static void mk_init(void)
{
#if MARKLIN_HW_UART3
	uart_config_and_enable_marklin();   /* UART3, 2400 baud, 8N2 */
#else
	auxuart_init();                     /* AUX mini-UART, 115200 baud */
#endif
}

/* ----------------------------------------------------------- server ----- */

static int marklin_server_tid = -1;
int MarklinkServerTid(void) { return marklin_server_tid; }

void marklin_server_entry(void)
{
	int tid;
	MarklinkMsg m;
	int ack = 0;

	marklin_server_tid = MyTid();
	RegisterAs(MARKLIN_SERVER_NAME);
	mk_init();

	for (;;) {
		Receive(&tid, (char *)&m, (int)sizeof(m));

		switch (m.type) {

		case MARKLIN_MSG_SET_SPEED:
			/* 2-byte speed command: [speed][address] */
			mk_putc((unsigned char)(m.speed & 0x0F));
			mk_putc((unsigned char)(m.addr & 0xFF));
			Reply(tid, (const char *)&ack, (int)sizeof(ack));
			break;

		case MARKLIN_MSG_SET_SWITCH:
			/* 3-byte solenoid command: [0x20|dir][address][0x00] */
			mk_putc((unsigned char)(MARKLIN_SW_BASE | (m.dir & 0x03)));
			mk_putc((unsigned char)(m.addr & 0xFF));
			mk_putc(0x00);
			Reply(tid, (const char *)&ack, (int)sizeof(ack));
			break;

		case MARKLIN_MSG_POLL_SENSORS: {
			/* Send 0x85 (read 5 modules at once); receive 10 bytes.
			 * One command/response round-trip instead of five. */
			MarklinkSensors s;
			mk_putc((unsigned char)(MARKLIN_SENSOR_BASE | MARKLIN_MODULES));
			for (int i = 0; i < (int)sizeof(s.data); i++)
				s.data[i] = mk_getc();
			Reply(tid, (const char *)&s, (int)sizeof(s));
			break;
		}

		default:
			Reply(tid, (const char *)&ack, (int)sizeof(ack));
			break;
		}
	}
}

/* ---------------------------------------------------- client stubs ----- */

void MarklinkSetSpeed(int train_addr, int speed)
{
	if (marklin_server_tid < 0) return;
	MarklinkMsg req;
	req.type  = MARKLIN_MSG_SET_SPEED;
	req.addr  = train_addr;
	req.speed = speed;
	req.dir   = 0;
	int ack;
	Send(marklin_server_tid, (const char *)&req, (int)sizeof(req),
	     (char *)&ack, (int)sizeof(ack));
}

void MarklinkSetSwitch(int sw_addr, int dir)
{
	if (marklin_server_tid < 0) return;
	MarklinkMsg req;
	req.type  = MARKLIN_MSG_SET_SWITCH;
	req.addr  = sw_addr;
	req.speed = 0;
	req.dir   = dir;
	int ack;
	Send(marklin_server_tid, (const char *)&req, (int)sizeof(req),
	     (char *)&ack, (int)sizeof(ack));
}

MarklinkSensors MarklinkPollSensors(void)
{
	MarklinkSensors s;
	for (int i = 0; i < (int)sizeof(s.data); i++) s.data[i] = 0;
	if (marklin_server_tid < 0) return s;
	MarklinkMsg req;
	req.type  = MARKLIN_MSG_POLL_SENSORS;
	req.addr  = 0;
	req.speed = 0;
	req.dir   = 0;
	Send(marklin_server_tid, (const char *)&req, (int)sizeof(req),
	     (char *)&s, (int)sizeof(s));
	return s;
}
