#include "processes.h"
#include "rpi.h"
#include "syscall.h"
#include "nameserver.h"
#include "custstr.h"
#include "gameserver.h"
#include "systimer.h"
#include "clockserver.h"

#include "ioserver.h"
#define DISPLAY 1
#define GETC 32
#define PUTC 33
#define CTS 34

/*
These are the most essential terminal control sequences that you will need for your train program.

Code	Effect
"\033[2J"	STATE the screen.
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

#define uartINTER 153
#define QUEUELENGTH 100

// this struct can be used to store the function call and interrupts
struct intFun
{
	uint8_t tid;	  // the PID of the send, put or CTS
	uint8_t type;	  // the type of the interrupt it is waiting for
	uint8_t channel;  // the channel of the interrupt it is waiting for
	uint8_t char_ch;  // the character that is being sent or recieved or the state of the CTS it is waiting for
	uint8_t char_ch2; // the character that is being sent or recieved or the state of the CTS it is waiting for if it is a 2 character marklin command
};
// this list contains all the functions that are waiting for a certain interrupt, if that interrupt fires then the function is unblocked
// all functions that are blocked by the same interrupt are stored in a list
struct fi_list
{
	struct intFun call[QUEUELENGTH];
	int size;
	uint8_t begin;
	uint8_t end;
};

// one queue to wait to be fired, another queue to await for the send to be returned
void io_TXIC_MARKLIN_server()
{
	// It is automatically assumed the channel is 2
	struct fi_list call_list;
	struct fi_list interrupt_list;
	RegisterAs("io_TXIC_MARKLIN_server");
	
	// set the size of the lists to 0
	// define STATE car
	// STATE: 0 not STATE: 1 STATE
	// set call list size to 0
	// set interrupt list size to 0
	call_list.size = 0;
	interrupt_list.size = 0;
	uint8_t STATE = 1;
	while (1)
	{
		int8_t io_notifier_tid = WhoIs("io_notifier");
		int tid;
		char recieve[8];
		Receive(&tid, recieve, 8);
		uint8_t type = recieve[0];
		uint8_t channel = recieve[1];
		uint8_t char_ch = recieve[2];
		uint8_t char_ch2 = recieve[3];
		
		if (io_notifier_tid == tid){
			Reply(tid, recieve, 0);
		}

		if(type == CTSMIM){
			if(call_list.call[call_list.begin].tid != 0 && call_list.size > 0 && char_ch == 1 && STATE == 3){
				interrupt_list.call[interrupt_list.end].tid = tid;
				interrupt_list.call[interrupt_list.end].type = type;
				interrupt_list.call[interrupt_list.end].channel = channel;
				interrupt_list.call[interrupt_list.end].char_ch = char_ch;
				interrupt_list.call[interrupt_list.end].char_ch2 = char_ch2;
				interrupt_list.end = (interrupt_list.end + 1) % QUEUELENGTH;
				interrupt_list.size++;	
			}
			if(call_list.call[call_list.begin].tid != 0 && call_list.size > 0 && char_ch == 0 && STATE == 2){
				STATE = 3;
				#if DISPLAY % 3== 0
					uart_printf(CONSOLE, "	STATE = %d\r\n", STATE);
				#endif
			}
		}
		if(type == TXIC){
			// add interrupt to the interrupt list
			if(call_list.call[call_list.begin].tid != 0 && call_list.size > 0 && STATE == 0){
				STATE = 2;
				#if DISPLAY % 3== 0
					uart_printf(CONSOLE, "	STATE = %d\r\n", STATE);
				#endif
				
			}
		} 
		
		if(type == PUTC){
			#if DISPLAY % 3== 0
				uart_printf(CONSOLE, "	type == PUTC\r\n");
			#endif
			call_list.call[call_list.end].tid = tid;
			call_list.call[call_list.end].type = type;
			call_list.call[call_list.end].channel = channel;
			call_list.call[call_list.end].char_ch = char_ch;
			call_list.call[call_list.end].char_ch2 = char_ch2;
			call_list.end = (call_list.end + 1) % QUEUELENGTH;
			call_list.size++;
			if(0){
				call_list.call[call_list.end].tid = tid;
				call_list.call[call_list.end].type = type;
				call_list.call[call_list.end].channel = channel;
				call_list.call[call_list.end].char_ch = char_ch2;
				call_list.call[call_list.end].char_ch2 = char_ch2;
				call_list.end = (call_list.end + 1) % QUEUELENGTH;
			}
		}
		// if there exist an interrupt to match up with a request
		if (call_list.call[call_list.begin].tid != 0 && call_list.size > 0 && STATE == 1)
		{
			STATE = 0;
			#if DISPLAY % 3== 0
				uart_printf(CONSOLE, "	STATE = %d\r\n", STATE);
			#endif
			uart_putc(MARKLIN, call_list.call[call_list.begin].char_ch);
		}
		
		if(call_list.size > 0 && interrupt_list.size > 0){
			int ret_pid = call_list.call[call_list.begin].tid;
			recieve[0] = interrupt_list.call[interrupt_list.begin].type;
			recieve[1] = interrupt_list.call[interrupt_list.begin].channel;
			recieve[2] = interrupt_list.call[interrupt_list.begin].char_ch;
			recieve[3] = interrupt_list.call[interrupt_list.begin].char_ch2;
			// print in green
			Reply(ret_pid, recieve, 8);
			call_list.call[call_list.begin].tid = 0;
			call_list.begin = (call_list.begin + 1) % QUEUELENGTH;

			call_list.size--;
			interrupt_list.begin = (interrupt_list.begin + 1) % QUEUELENGTH;
			interrupt_list.size--;
			STATE = 1;
			#if DISPLAY % 3== 0
				uart_printf(CONSOLE, "	STATE = %d\r\n", STATE);
			#endif
		}
	}
	
	Exit();
}
void io_RXIC_MARKLIN_server()
{
	// It is automatically assumed the channel is 2
	int tid;
	RegisterAs("io_RXIC_MARKLIN_server");
	
	// for the recieve interrupts I need to handle cases in which the interrupt happened before a task picked it up
	// this doubles of as a queue for the interrupts
	struct fi_list interrupt_list;
	struct fi_list call_list;
	// set the size of the lists to 0
	// 0 is nothing
	// 1 is CONSOLE
	// 2 is MARKLIN

	// need to consider cases in which the interrupt arrived before the task picked it up
	// in other words the task is still blocked on the AwaitEvent or a PutC
	while (1)
	{
		// change font to orange
		int8_t io_notifier_tid = WhoIs("io_notifier"); 
		char recieve[8];
		Receive(&tid, recieve, 8);
		uint8_t type = recieve[0];
		uint8_t channel = recieve[1];
		uint8_t char_ch = recieve[2];
		// get arrives after interrupt
		// interrupt arrives after get
		if (io_notifier_tid == tid){
			Reply(tid, recieve, 0);
		}
		if (type == RXIC)
		{
			#if DISPLAY % 3== 0
				uart_printf(CONSOLE, "	RXIC FUNCTION tid = %u, char_ch = %u\r\n", tid, char_ch);
			#endif
			interrupt_list.call[interrupt_list.end].tid = tid;
			interrupt_list.call[interrupt_list.end].type = type;
			interrupt_list.call[interrupt_list.end].channel = channel;
			interrupt_list.call[interrupt_list.end].char_ch = char_ch;
			interrupt_list.end = (interrupt_list.end + 1) % QUEUELENGTH;
			interrupt_list.size++;
		}
		else if (type == GETC)
		{
			#if DISPLAY % 3== 0
				uart_printf(CONSOLE, "	GETC FUNCTION tid = %u, char_ch = %u\r\n", tid, char_ch);
			#endif
			// check is the channel is empty
			// # if DISPLAY == 4 uart_printf(CONSOLE, "GETC FUNCTION char_ch = 0x%x, tid = %u\r\n", char_ch, tid);
			call_list.call[call_list.end].tid = tid;
			call_list.call[call_list.end].type = type;
			call_list.call[call_list.end].channel = channel;
			call_list.call[call_list.end].char_ch = char_ch;
			call_list.end = (call_list.end + 1) % QUEUELENGTH;
			call_list.size++;
		}
		// if there exist an interrupt to match up with a request
		if (call_list.size > 0 && interrupt_list.size > 0)
		{
			int ret_pid = call_list.call[call_list.begin].tid;
			recieve[0] = interrupt_list.call[interrupt_list.begin].type;
			recieve[1] = interrupt_list.call[interrupt_list.begin].channel;
			recieve[2] = interrupt_list.call[interrupt_list.begin].char_ch;
			recieve[3] = interrupt_list.call[interrupt_list.begin].char_ch2;
			// # if DISPLAY == 4 uart_printf(CONSOLE, "\033[37m");
			Reply(ret_pid, recieve, 8);
			call_list.call[call_list.begin].tid = 0;
			call_list.begin = (call_list.begin + 1) % QUEUELENGTH;
			call_list.size--;
			interrupt_list.begin = (interrupt_list.begin + 1) % QUEUELENGTH;
			interrupt_list.size--;
		}
		// print in white
		// # if DISPLAY == 4 uart_printf(CONSOLE, "\033[37m");
	}
	Exit();
}
void io_CTS_MARKLIN_server()
{
	// It is automatically assumed the channel is 2
	// awaitcts[0] is awaiting for down, awaitcts[1] is awaiting for up
	uint32_t awaitcts[2][NUMPROCS]; 
	
	// awaitcts_size[0] is the size of the list that is awaiting for down, 
	// awaitcts_size[1] is the size of the list that is awaiting for up
	uint32_t awaitcts_size[2]; 
	// register the server
	RegisterAs("io_CTS_MARKLIN_server");
	
	uint8_t STATE = 0;
	while (1)
	{
		int8_t io_notifier_tid = WhoIs("io_notifier");
		int tid;
		char recieve[8];
		Receive(&tid, recieve, 8);
		uint8_t type = recieve[0];
		uint8_t channel = recieve[1];
		uint8_t char_ch = recieve[2];
		/* code */
		if (io_notifier_tid == tid){
			Reply(tid, recieve, 0);
		}
		if(type ==CTSMIM){
			# if DISPLAY % 3== 0
				uart_printf(CONSOLE, "	CTS = %d\r\n", char_ch);
			#endif
			int cur_cts = get_CTS(MARKLIN);
			for (int i = 0; i < NUMPROCS; i++){
				int tid_free = awaitcts[cur_cts][i];
				recieve[2] = cur_cts;
				Reply(tid_free, recieve, 8);
			}
			awaitcts_size[cur_cts] = 0;
		} else if(type == CTS){
			# if DISPLAY % 3== 0 
				uart_printf(CONSOLE, "	CTS FUNCTION tid = %u, char_ch = %u\r\n", channel, tid, char_ch);
			#endif
			if (char_ch == get_CTS(MARKLIN)){
				Reply(tid, recieve, 8);
			} else {
				awaitcts[char_ch][awaitcts_size[char_ch]] = tid;
				awaitcts_size[char_ch]++;
			}
		}
	}
	
	Exit();
}

void io_notifier()
{
	// The handler in the kernel only handels different type of interrupt of the same ID
	// However in between different interrupts of the same ID, it is handeled in here
	
	RegisterAs("io_notifier");
	# if DISPLAY % 3== 0
	uart_printf(CONSOLE, "io_notifier registered\r\n");
	#endif
	int io_TXIC_MARKLIN_server_tid = -1;
	int io_RXIC_MARKLIN_server_tid = -1;
	int io_CTS_MARKLIN_server_tid = -1;
	// print in green IO notifier is running
	uart_printf(CONSOLE, "\033[32m");
	uart_printf(CONSOLE, "io_notifier: Started\r\n\033[37m");
	while (1)
	{
		io_TXIC_MARKLIN_server_tid = WhoIs("io_TXIC_MARKLIN_server");
		io_RXIC_MARKLIN_server_tid = WhoIs("io_RXIC_MARKLIN_server");
		io_CTS_MARKLIN_server_tid = WhoIs("io_CTS_MARKLIN_server");
		uint64_t event = AwaitEvent(uartINTER);
		# if DISPLAY % 3== 0
		uart_printf(CONSOLE, "io_notifier: event = %d\r\n", event);
		#endif
		int ret;
		// the 0 th byte is the interrupt id

		uint8_t type = event & 0xFF;
		uint8_t channel = (event >> 8) & 0xFF;
		uint8_t char_ch = (event >> 16) & 0xFF;
		if (channel == CONSOLE){
			// there exist no server for the console
		}
		if (channel == MARKLIN)
		{
			if (type == RXIC)
			{
				# if DISPLAY % 3== 0
				uart_printf(CONSOLE, "io_notifier: RXIC SYSINTERRUPT\r\n");
				#endif
				Send(io_RXIC_MARKLIN_server_tid, &event, 8, &ret, 0);
			}
			else if(type == TXIC)
			{
				# if DISPLAY % 3== 0
				uart_printf(CONSOLE, "io_notifier: TXIC SYSINTERRUPT\r\n");
				#endif
				Send(io_TXIC_MARKLIN_server_tid, &event, 8, &ret, 0);
			}
			else if(type == CTSMIM)
			{
				# if DISPLAY % 3== 0
				uart_printf(CONSOLE, "io_notifier: CTSMIM SYSINTERRUPT\r\n");
				#endif
				Send(io_CTS_MARKLIN_server_tid, &event, 8, &ret, 0);
				Send(io_TXIC_MARKLIN_server_tid, &event, 8, &ret, 0);
			}
		}
	}
	Exit();
}


/*
int Getc(int tid, int channel)
returns the next un-returned character from the given channel.
The first argument is the task id of the appropriate I/O server.
How communication errors are handled is implementation-dependent.
Getc() is actually a wrapper for a send to the appropriate server.
Return Value
>=0	new character from the given uart.
-1	tid is not a valid uart server task.
*/
// have the server look out for the most recent interrupt that is the RXIC on the marklin
// get a character from the terminal, the server would reply with the character

// the send character would be in 8 bytes
// secment 0 - 31 into 8 bytes in bits
// 0 - 7: TYPE CTSMIM, RXIC, TXIC, GETC, PUTC...
// 8 - 15: Console
// 16 - 23: the return or put character
// 24 - 31
// 32 - 39
// 40 - 47
// 48 - 55
// 56 - 63 : Identifier bits, those bits identifies the command issued to the server
int Getc(int tid, int channel)
{
	char channel64[8];
	// uint8_t channel8 = (uint8_t) channel;
	// *((uint32_t *) channel64 + 1) = ((uint32_t) channel);
	channel64[0] = GETC;
	channel64[1] = (uint8_t)channel;
	channel64[2] = 0;
	channel64[3] = -1;
	uint64_t sendret = Send(tid, &channel64, 8, &channel64, 8);

		// # if DISPLAY == 4 uart_printf(CONSOLE, "GETC: sendret = %d\r\n", sendret);
	return channel64[2];
}
/*
int Putc(int tid, int channel, unsigned char ch)
queues the given character for transmission by the given uart.
On return the only guarantee is that the character has been queued.
Whether it has been transmitted or received is not guaranteed.
How communication errors are handled is implementation-dependent.
Putc() is actually a wrapper for a send to the appropriate server.
Return Value
0	success.
-1	tid is not a valid uart server task.
*/
// Either the queue is empty or the server needs to wait for the TXIC interrupt to be triggered

// 0 - 7: TYPE CTSMIM, RXIC, TXIC, GETC, PUTC...
// 8 - 15: Console
// 16 - 23: the return or put character
// 24 - 31
// 32 - 39
// 40 - 47
// 48 - 55
// 56 - 63 : Identifier bits, those bits identifies the command issued to the server
int Putc(int tid, int channel, unsigned char ch)
{
	char channel64[8];
	*((uint32_t *)channel64 + 1) = ((uint32_t)channel);
	channel64[0] = PUTC;
	channel64[1] = (uint8_t)channel;
	channel64[2] = ch;
	channel64[3] = -1;
	uint64_t sendret = Send(tid, &channel64, 8, &channel64, 8);
	#if DISPLAY == 3
	 uart_printf(CONSOLE, "Putc: sendret = %d\r\n", sendret);
	#endif
	return channel64[2];
}
// cannot get over the waitCTS thing. I want the my code to unblock when the CTS is high
// this way the marklin would not swallow commands too fast
int Put2c(int tid, int channel, unsigned char ch, unsigned char ch2)
{
	char channel64[8];
	*((uint32_t *)channel64 + 1) = ((uint32_t)channel);
	channel64[0] = PUTC;
	channel64[1] = (uint8_t)channel;
	channel64[2] = ch;
	channel64[3] = ch2;
	uint64_t sendret = Send(tid, &channel64, 8, &channel64, 1);

	#if DISPLAY == 2 
	uart_printf(CONSOLE, "Put2c: sendret = %d\r\n", sendret);
	#endif
	return channel64[2];
}

int awaitCTS(int tid, int channel, uint8_t val)
{	
	// print the params
	# if DISPLAY % 3== 0
	 uart_printf(CONSOLE, "awaitCTS: tid = %u, channel = %u, val = %u\r\n", tid, channel, val);
	#endif
	char channel64[8];
	*((uint32_t *)channel64 + 1) = ((uint32_t)channel);
	channel64[0] = CTS;
	channel64[1] = (uint8_t)channel;
	channel64[2] = val;
	channel64[3] = -1;
	uint64_t sendret = Send(tid, &channel64, 8, &channel64, 8);
	# if DISPLAY % 3== 0 
	uart_printf(CONSOLE, "awaitCTS: sendret = %d\r\n", sendret);
	#endif
	return channel64[2];
}