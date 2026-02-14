#include "rpi.h"
#include "syscall.h"
#include "processes.h"
#include "gic.h"
#include "layer0.h"
#include "config.h"

void run_layer1_tests(void);
void run_layer2_tests(void);

void idle(){
	int count = 0;
	while(1){
		uint32_t runtime = GetRuntime();
		uint32_t kernelrt = GetKernelRuntime();
    // print the column and row onto 2 and 1
    uart_printf(CONSOLE, "\033[2;1H");
		uart_printf(CONSOLE, "idle: [%d] runprecentage = %u %% \r\n", count++, (100 * runtime) / kernelrt);
		wfi();
	}
	Exit();
}

void master_test_runner() {
	// Run Layer 1 tests first
	run_layer1_tests();
	
	// Then run Layer 2 tests
	run_layer2_tests();
	
	Exit();
}

int kmain(void *reg) {
  /* Early boot diagnostics */
  uart_init();
  uart_config_and_enable(CONSOLE, 115200);
  
  uart_printf(CONSOLE, "\r\n");
  uart_printf(CONSOLE, "\033[1;36m[====] Galatea-NIX Kernel Boot\033[0m\r\n");
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m UART initialized at 115200 baud\r\n");

  InitSys(reg);
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Kernel subsystems initialized\r\n");
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Process scheduler ready\r\n");
  
  #if CLOCKSERVERON == 1
  // Basic timer setup
  route_interrupt(CLOCKINTID, 0);
  enable_interrupt(CLOCKINTID);
  uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m System timer configured (IRQ %d)\r\n", CLOCKINTID);
  #endif

  // Create master test runner task
  uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Starting test suite...\r\n");
  int tid = KernelCreate(10, master_test_runner, 0);
  (void)tid;

  Schedule();
  // U-Boot displays the return value from main - might be handy for debugging

  return 0;
}
