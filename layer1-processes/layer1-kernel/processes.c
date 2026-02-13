#include "processes.h"
#include "rpi.h"
#include "asm.h"
#include "syscall.h"

#include "custstr.h"
#include "nameserver.h"
#include "gameserver.h"
#include "clockserver.h"

#include "systimer.h"
#include "tests/tc1tests.h"
#include "tc1/marklin_worker.h"
#include "ioserver.h"

#define DISPLAY 1
/*
These are the most essential terminal control sequences that you will need for your train program.

Code	Effect
"\033[2J"	Clear the screen.
"\033[H"	Move the cursor to the upper-left corner of the screen.
"\033[r;cH"	Move the cursor to row r, column c. Note that both the rows and columns are indexed starting at 1.
"\033[?25l"	Hide the cursor.
"\033[K"	Delete everything from the cursor to the end of the line.
These control sequences can help make your program's display more lively.

Code	Effect
"\033[0m"	Reset special formatting (such as colour).
"\033[30m"	Black text.
"\033[31m"	Red text.
"\033[32m"	Green text.
"\033[33m"	Yellow text.
"\033[34m"	Blue text.
"\033[35m"	Magenta text.
"\033[36m"	Cyan text.
"\033[37m"	White text.

*/

#define UARTINTER 153
void init_trains(){
  Exit();
}

// new paradymn, run tests for each k# assignment (other than 3) before running the shell
void init_solonoids() // First task as dictated in the reqs
{	// need to set the timer interrupt
	RegisterAs("init_solonoids");
	#define SWITCH_COUNT 18
	int tid;
	//void set_solonoid(int marklin_worker_tid, uint8_t sol_id, char state);
	int marklin_worker_tid = WhoIs("marklin_worker");
  // set all the turnabouts to straight
	for (uint8_t i = 1; i <= SWITCH_COUNT; i ++){
		// set_solonoid(marklin_worker_tid, i, 'S');
		// this command only enqueues the switches
	}
	// uart_printf(CONSOLE,"\033[%u;%uHSWITCHES ALL STRAIGHT:",TOP_ROW + COMMAND_ROW, LEFT_COL + 1);
	// set all the turnabouts to curved
	for (uint8_t i = 1; i <= SWITCH_COUNT; i ++){
		set_solonoid(marklin_worker_tid, i, 'C');
	}
	/*
	set_solonoid(marklin_worker_tid, 0x99, 'S');
	set_solonoid(marklin_worker_tid, 0x9a, 'C');
	set_solonoid(marklin_worker_tid, 0x9b, 'S');
	set_solonoid(marklin_worker_tid, 0x9c, 'C');
	*/
	set_solonoid(marklin_worker_tid, 0x99, 'C');
	set_solonoid(marklin_worker_tid, 0x9a, 'S');
	set_solonoid(marklin_worker_tid, 0x9b, 'C');
	set_solonoid(marklin_worker_tid, 0x9c, 'S');
	// int speed_measuring_tid = Create(3, speed_gather);
	Exit();
}