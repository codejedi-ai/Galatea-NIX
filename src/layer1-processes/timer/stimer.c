/*
 * Simple timer: sleep/snooze helpers using system timer compare registers.
 * Used for UART/line timing (e.g. stimer_creset per line).
 */

#include "stimer.h"
#include "mmio_config.h"

#define STIMER_CS  0x00
#define STIMER_CLO 0x04
#define STIMER_CHI 0x08
#define STIMER_C0  0x0c
#define STIMER_C1  0x10
#define STIMER_C2  0x14
#define STIMER_C3  0x18

static char *const STIMER_BASE = (char *)(CONFIG_STIMER_BASE);
#define STIMER_REG(offset) (*(volatile uint32_t *)(STIMER_BASE + (offset)))

static const uint32_t CS_M0 = 0x00;
static const uint32_t CS_M1 = 0x10;
static const uint32_t CS_M2 = 0x20;
static const uint32_t CS_M3 = 0x40;
static const uint32_t STIMER_INT = 5000;

static uint32_t STIMER_SNOOZE;

uint32_t stimer_getlo(void)
{
	return STIMER_REG(STIMER_CLO);
}

uint32_t stimer_gethi(void)
{
	return STIMER_REG(STIMER_CHI);
}

void stimer_snooze(void)
{
	STIMER_SNOOZE = stimer_getlo() + STIMER_INT;
}

void stimer_wake(void)
{
	while ((int)(STIMER_SNOOZE - stimer_getlo()) > 0)
		;
}

void stimer_creset(size_t line)
{
	(void)line;
	/* Clear compare status for line if needed; no-op for now. */
}
