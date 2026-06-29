#include "config.h"
#include "mmio_config.h"

/*
 * Central hardware / board configuration for d273liu-nix.
 *
 * This is the single runtime home for hardware config. The compile-time macros
 * in config.h and mmio_config.h still exist because C needs them for #if
 * conditions, array sizes, case labels, and static initialisers (e.g. rpi.c's
 * `static char *const UART0_BASE = (char*)CONFIG_UART0_BASE;`). BOARD mirrors
 * those values as one const object that drivers and services read at runtime —
 * the AUX/link driver, the screen layer, and main() all source their hardware
 * parameters from here.
 */
const board_config_t BOARD = {
	.platform     = CONFIG_PLATFORM_NAME,
	.mmio_base    = (uintptr_t)CONFIG_MMIO_BASE,
	.uart0_base   = (uintptr_t)CONFIG_UART0_BASE,
	.uart3_base   = (uintptr_t)CONFIG_UART3_BASE,
	.aux_base     = (uintptr_t)CONFIG_AUX_BASE,
	.gic_base     = (uintptr_t)CONFIG_GIC_BASE,
	.gpio_base    = (uintptr_t)CONFIG_GPIO_BASE,
	.console_baud = 115200,
	.marklin_baud = 2400,
	.link_port    = 6011,
};
