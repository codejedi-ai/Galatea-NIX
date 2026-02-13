#include "rpi.h"
#include "syscall.h"
#include "processes.h"
#include "gic.h"
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

  // Initialize basic system services
  int tid = KernelCreate(-1, idle, 0);
  
  #if CLOCKSERVERON == 1
  // Basic timer setup
  route_interrupt(CLOCKINTID, 0);
  enable_interrupt(CLOCKINTID);
  #endif

  // Create a basic first user task
  tid = KernelCreate(1, first_user_task, 0);

  Schedule();
  // U-Boot displays the return value from main - might be handy for debugging

  return 0;
}
