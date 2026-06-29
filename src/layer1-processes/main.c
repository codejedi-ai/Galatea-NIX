#include "rpi.h"
#include "syscall.h"
#include "processes.h"
#include "gic.h"
#include "../layer3-services/uart/io_api/io_api.h"
#include "../layer3-services/uart/UART1_CONSOLE_server/UART1_CONSOLE_server.h"
#include "../layer3-services/uart/io_notifier/io_notifier.h"
#include "layer0.h"
#include "config.h"
#include "project.h"
#include "terminal_shell.h"
#include "vmm.h"
#include "pmm.h"
#include "idle.h"
#include "nameserver.h"
#include "timer/systimer.h"
#include "accel.h"
#include "apu_completion.h"
#include "eventbus.h"
#if CLOCKSERVERON == 1
#include "clockserver.h"
#include "apu_core_server.h"
#include "rps.h"
#if ENABLE_LINK == 1
#include "linkserver.h"
#endif
#if ENABLE_MARKLIN == 1
#include "marklin.h"
#endif
#endif

int kmain(void *reg) {
  uart_init();
  uart_config_and_enable(CONSOLE, BOARD.console_baud);

  uart_printf(CONSOLE, "\r\n");
  uart_printf(CONSOLE, "\033[1;36m[====] %s Kernel Boot\033[0m\r\n", PROJECT_DISPLAY_NAME);
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m UART initialized at 115200 baud\r\n");

  __asm__ volatile ("msr cntkctl_el1, %0\n isb" :: "r"((uint64_t)3));

  InitSys(reg);
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Kernel subsystems initialized (no heap)\r\n");

  {
    extern void secondary_entry(void);
    uint64_t entry = (uint64_t)(uintptr_t)secondary_entry;
    *(volatile uint64_t *)0xE0 = entry;
    *(volatile uint64_t *)0xE8 = entry;
    *(volatile uint64_t *)0xF0 = entry;
    __asm__ volatile("dsb sy\n sev\n" ::: "memory");
    accel_init();
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Accelerator cores 1-3 online\r\n");
  }
  eventbus_init();
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Event bus initialized\r\n");
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Memory: %u-page frame pool\r\n",
              (unsigned)pmm_total_pages());
  vm_selftest();
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Process scheduler ready\r\n");

#if CLOCKSERVERON == 1
  gic_init();
  apu_completion_init();
  route_interrupt(UARTINTER, 0);
  enable_interrupt(UARTINTER);
  enable_RX_and_TX();
  KernelCreate(0, io_notifier, 0);
  KernelCreate(0, UART2_MARKLIN_server, 0);
  KernelCreate(0, UART1_CONSOLE_server, 0);
  enable_interrupt(CLOCKINTID);
  gentimer_arm_ms(10);
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Generic timer tick configured (PPI %d)\r\n", CLOCKINTID);

  KernelCreate(NAME_SERVER_PRIORITY, name_server_entry, 0);
  /* Three per-core APU servers — each owns one secondary core. */
  KernelCreate(APU_SERVER_PRIORITY, apu1_server_entry, 0);
  KernelCreate(APU_SERVER_PRIORITY, apu2_server_entry, 0);
  KernelCreate(APU_SERVER_PRIORITY, apu3_server_entry, 0);
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m APU servers: %s, %s, %s\r\n",
              APU1_SERVER_NAME, APU2_SERVER_NAME, APU3_SERVER_NAME);

  KernelCreate(CLOCK_SERVER_PRIORITY, clock_server_entry, 0);
  KernelCreate(RPS_SERVER_PRIORITY, rps_server_entry, 0);
#if ENABLE_LINK == 1
  KernelCreate(LINK_SERVER_PRIORITY, link_server_entry, 0);
#endif
#if ENABLE_MARKLIN == 1
  KernelCreate(MARKLIN_SERVER_PRIORITY, marklin_server_entry, 0);
#endif
  KernelCreate(IDLE_PRIORITY, idle_entry, 0);
#endif

#if START_SHELL == 1
  KernelCreate(20, terminal_shell_entry, 0);
#endif

#if CLOCKSERVERON != 1
  KernelCreate(IDLE_PRIORITY, idle_entry, 0);
#endif

  Schedule();
  return 0;
}
