#include "rpi.h"
#include "syscall.h"
#include "processes.h"
#include "nameserver.h"
#include "clockserver.h"
#include "ioserver.h"
#include "gameserver.h"

#include "gic.h"

#include "layer3-application/marklin_worker.h"
#include "layer3-application/track_server.h"
#include "layer3-application/shell.h"

// Include test framework
#include "layer3-application/tests/test_runner.h"
void* STACK_EL0_START; // Maybe delete this later
#define CLOCKINTID 99
#define CLOCKSERVERON 1
#define UARTSERVERON 1
void idle(){
	while(1){
    		// uart_printf(CONSOLE, "idle: WFI <Print time here>\r\n");
		// uart_printf(CONSOLE, "idle: time = %u\r\n", time);
		uint32_t runtime = GetRuntime();
		uint32_t kernelrt = GetKernelRuntime();
    // print the column and row onto 2 and 1
    uart_printf(CONSOLE, "\033[2;1H");
		uart_printf(CONSOLE, "idle: runprecentage = %u \% \r\n", (100 * runtime) / kernelrt);
		asm("WFI");
	}
	Exit();
}

int kmain(void *reg) {

  STACK_EL0_START = reg; // Immediately calls this to store stack_end point as x0
  InitSys(reg);

  uart_init();
  uart_config_and_enable(CONSOLE, 115200);
  uart_config_and_enable_marklin();

  // Run comprehensive test suite first
  uart_printf(CONSOLE, "Starting Galatea-NIX Kernel Self-Tests...\n");
  run_all_layer_tests();
  uart_printf(CONSOLE, "Kernel Self-Tests Complete.\n\n");

  // Continue with normal system initialization
  int tid = KernelCreate(0, nameserver, 0);
  tid = KernelCreate(-1, idle, 0);
  #if CLOCKSERVERON == 1
  set_timerC3(get_timerLO() + 10000);
  route_interrupt(CLOCKINTID, 0);
  enable_interrupt(CLOCKINTID);
  tid = KernelCreate(0, clock_notifier, 0);
  tid = KernelCreate(0, clock_server, 0);
  #endif
  #if UARTSERVERON == 1
  // enable and route the interupt
  route_interrupt(UARTINTER, 0);
  enable_interrupt(UARTINTER);
  enable_RX_and_TX();


  // INIT THE IO-SERVERS AND NOTIFIERS
  tid = KernelCreate(0, io_notifier, 0);
  uart_printf(CONSOLE, "io_notifier tid: %d\r\n", tid);
  tid = KernelCreate(0, io_TXIC_MARKLIN_server, 0);
  uart_printf(CONSOLE, "io_TXIC_MARKLIN_server tid: %d\r\n", tid);
  tid = KernelCreate(0, io_RXIC_MARKLIN_server, 0);
  uart_printf(CONSOLE, "io_RXIC_MARKLIN_server tid: %d\r\n", tid);
  tid = KernelCreate(0, io_CTS_MARKLIN_server, 0);
  uart_printf(CONSOLE, "io_CTS_MARKLIN_server tid: %d\r\n", tid);
  #endif


  // tid = KernelCreate(0, switch_worker, 0);

  //uart_printf(CONSOLE, "init_solonoids\r\n", tid);


  uart_printf(CONSOLE, "idle tid: %d\r\n", tid);
  // create first user task
  tid = KernelCreate(2, init_solonoids, 0);
    // sensor servers


  tid = KernelCreate(1, marklin_worker_read_notifier, 0);
  uart_printf(CONSOLE, "marklin_worker_read_notifier tid: %d\r\n", tid);
  tid = KernelCreate(1, marklin_worker, 0);
  uart_printf(CONSOLE, "marklin_worker tid: %d\r\n", tid);
  //uart_printf(CONSOLE, "marklin_worker tid: %d\r\n", tid);
  // clear the screen

  tid = KernelCreate(1, track_server, 0);
  uart_printf(CONSOLE, "track_server tid: %d\r\n", tid);

  KernelCreate(-2, command_shell, 0);
  uart_printf(CONSOLE, "\033[2J");
  // switch worker
  Schedule();
  // U-Boot displays the return value from main - might be handy for debugging

  return 0;
}
