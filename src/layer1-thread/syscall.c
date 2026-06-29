#include "syscall.h"
#include "processes.h"
#include "asm.h"
#include "layer0.h"
#include "rpi.h"
#include "malloc.h"
#include "util.h"
#include "math.h"
#include "timer/systimer.h"
#include "gic.h"
#include "../layer2-messaging/messaging.h"

// Forward declarations for functions that may be missing
extern void EXIT();
extern void setActiveInterrupt(uint32_t interruptid);
extern void INTERRUPT_CLEAR_ACTIVE_REGS(uint32_t interruptid);
extern uint32_t readInterruptId();
#include "rpi.h"
extern unsigned char uart_getc_modified(size_t line);
extern uint32_t get_CTS(size_t line);
extern void clear_GICC_EOIR(uint16_t interrupt_id);
extern uint32_t checkActiveInterrupt(uint32_t interrupt_id);
extern int KernelCreate(uint8_t priority, void (*function)(), int parent);
extern void recieve_helper(int PID);
extern void handlerExceptionHelper(uint64_t esr_el1);
# define READY 0
# define BLOCKED 1

// Kernel-internal data structures
// Note: PID and PROCS are exposed to layer2-messaging for IPC
static void *STACKSTART;
uint32_t PID = 0;
struct process PROCS[NUMPROCS];
static struct process_container PROCESS_CONTAINERS[NPROCESSES];
static uint8_t PROCESS_SHARED_MEM[NPROCESSES][SHARED_MEM_PER_PROCESS];
static struct state READY_QUEUE[NUMPROCS];
static struct state BLOCKED_LIST[NUMPROCS];
static struct ReadyList READY_LIST;
static struct interrupt AWAIT_INTERRUPT[MAXEVENT];

uint32_t kernelStartTime = 0;
uint8_t NO_PARAMS = 0;

// SIMPLE FIFO READY LIST
// The kernel ready queue is a plain round-robin list: push appends at the
// tail, pop removes from the head. Scheduling order is insertion order;
// priority is no longer used to order the list (any AI/Q-learning policy now
// lives under servers/, decoupled from the kernel).

// append to the tail of the list
void ready_list_push( struct ReadyList *h, struct state k)
{
	if (h->size >= h->capacity)
	{
		return;
	}
	h->items[h->size] = k;
	h->size++;
}
// check if the list is empty
uint8_t ready_list_empty( struct ReadyList *h)
{
	return h->size == 0;
}
// remove and return the head of the list (FIFO), shifting the rest down
struct state ready_list_pop( struct ReadyList *h)
{
	if (h->size == 0)
		return (struct state){-1, 0, 0}; // Initialize all fields of the struct
	struct state head = h->items[0];
	for (unsigned i = 1; i < h->size; i++)
		h->items[i - 1] = h->items[i];
	h->size--;
	return head;
}

//SIMPLE FIFO READY LIST END

// scrSchedule(pid, priority)
void scrSchedule(int pid, uint8_t priority)
{

	struct state currItem = {pid, priority, ((uint64_t)get_timerHI() << 32) + get_timerLO()};
	/*
	struct state nextItem;
	int insert = 0;
	for (int i = 0; i < NUMPROCS; i++) {
		if (READY_QUEUE[i].pid == 0) {
			READY_QUEUE[i] = currItem;
			return 0;
		}
		else if (READY_QUEUE[i].priority > priority) {
			insert = 1;
		}
		if (insert) {
			nextItem = READY_QUEUE[i];
			READY_QUEUE[i] = currItem;
			currItem = nextItem;
		}
	}
	*/
	// put the process at the tail of the ready list
	ready_list_push(&READY_LIST, currItem);
}

// scrSchedule(pid, priority)
int unblock_ind(int pid, uint8_t priority)
{
	/*
	// uart_printf(CONSOLE, "unblock: pid = %u priority = %u ready =%u\r\n", pid, priority);
	struct state currItem = {pid, priority, ready};
	struct state nextItem;
	int insert = 0;
	for (int i = 0; i < NUMPROCS; i++) {
		if (READY_QUEUE[i].pid == 0) {
			// reached the end of the queue so ret
			return 0;	
		}
		else if (READY_QUEUE[i].priority == priority && READY_QUEUE[i].pid == pid) {
			READY_QUEUE[i].time = ready;
		}
	}
	*/
	if(BLOCKED_LIST[pid - 1].pid == 0 && BLOCKED_LIST[pid - 1].priority == priority) return 0;
	int p = PID - 1;
	(void)p; // Unused
	scrSchedule(pid, priority);
	BLOCKED_LIST[pid - 1].pid = 0;
	return 0;
}
/*
scrSchedule(PID, PROCS[p].priority, BLOCKED);
block(PID, PROCS[p].priority);
					*/
void block(int pid, uint8_t priority){
	// uart_printf(CONSOLE, "block: pid = %u priority = %u\r\n", pid, priority);
	//scrSchedule(pid, priority, BLOCKED);
	BLOCKED_LIST[pid - 1].pid = pid;
	BLOCKED_LIST[pid - 1].priority = priority;
	BLOCKED_LIST[pid - 1].time = BLOCKED;

}
void unblock(struct state currItem)
{
	// uart_printf(CONSOLE, "unblock: pid = %u priority = %u ready =%u\r\n", pid, priority);
	int pid = currItem.pid;
	uint8_t priority = currItem.priority;
	int ready = currItem.time;
	(void)ready; // Unused
	// uart_printf(CONSOLE, "unblock: pid = %u priority = %u ready =%u\r\n", pid, priority);
	
	unblock_ind(pid, priority);
}

int scrPick()
{
	int pid = -1;
	#if DEBUG_EXIT >= 1
	uart_printf(CONSOLE, "[DEBUG] scrPick: READY_LIST size=%u\r\n", READY_LIST.size);
	#endif
	if (ready_list_empty(&READY_LIST)) return -1;

	struct state currItem = ready_list_pop(&READY_LIST);
	pid = currItem.pid;
	#if DEBUG_EXIT >= 1
	uart_printf(CONSOLE, "[DEBUG] scrPick: picked PID=%d, priority=%u\r\n", pid, currItem.priority);
	#endif
	return pid;
}

void InitSys(void* reg)
{	// For some reason, normal init to 0 just.. doesn't work?
	kernelStartTime = get_timerLO();
	READY_LIST.size = 0;
	READY_LIST.capacity = NUMPROCS;
	READY_LIST.items = READY_QUEUE;
	STACKSTART = reg;
	PID = 0;
	malloc_init_default();  /* heap in BSS (RAM); 0x1000000 not valid on QEMU virt */
	for (int event = 0; event < MAXEVENT; event++){
		AWAIT_INTERRUPT[event].len = 0;
		AWAIT_INTERRUPT[event].eventq_len = 0;
		AWAIT_INTERRUPT[event].eventq_head = 0;
		AWAIT_INTERRUPT[event].eventq_tail = 0;
		for (int jdx = 0; jdx < NUMPROCS; jdx++) {
			AWAIT_INTERRUPT[event].pid_ls[jdx].pid = 0;
			AWAIT_INTERRUPT[event].pid_ls[jdx].priority = 0;
			AWAIT_INTERRUPT[event].pid_ls[jdx].time = 0;
		}
	}
	for (int i = 0; i < NPROCESSES; i++) {
		PROCESS_CONTAINERS[i].pid = i;
		PROCESS_CONTAINERS[i].shared_mem_base = &PROCESS_SHARED_MEM[i][0];
		PROCESS_CONTAINERS[i].shared_mem_size = SHARED_MEM_PER_PROCESS;
	}
	for (int idx = 0; idx < NUMPROCS; idx++) {
		PROCS[idx].stackpointer = NULL;
		PROCS[idx].pcpointer = NULL;
		PROCS[idx].pstate = 0;
		PROCS[idx].parentpid = 0;
		PROCS[idx].process_id = 0;
		PROCS[idx].priority = 0;
		// PROCS[idx].queuesize = 0;
		for (int jdx = 0; jdx < 31; jdx++) {
			PROCS[idx].registervalues[jdx] = 10 + jdx;
		
		}
		BLOCKED_LIST[idx].pid = 0;
		BLOCKED_LIST[idx].priority = 0;
		BLOCKED_LIST[idx].time = 0;

		READY_QUEUE[idx].pid = 0;
		READY_QUEUE[idx].time = -1;
		READY_QUEUE[idx].priority = -1;
		
		
	}
}

void updateRunTimer(){
	int p = PID - 1;
	uint32_t curtime = get_timerLO();
	PROCS[p].totaltime += curtime - PROCS[p].waketime;
}
// ============================ Handel Async

void HandleASYNC(void* sp) // A helper function to pull some c variables into assembly
{
	// We just arrived here, there is stuff on the stack that I do not want to deal with
	int p = PID - 1;
	updateRunTimer();
	#if DEBUG == 3 
	// uart_printf(CONSOLE, "HandleASYNC: Handling %x %x %x %x %x\r\n", sp, &PROCS[p].registervalues[0], &PROCS[p].pcpointer, &PROCS[p].stackpointer, &PROCS[p].pstate);
	#endif
	
	uint64_t ASYNC = Save(sp, &PROCS[p].registervalues[0], &PROCS[p].pcpointer, &PROCS[p].stackpointer, &PROCS[p].pstate);
	ExceptionASYNC(ASYNC);
	Schedule();

    #if DEBUG >= 1
		uart_printf(CONSOLE, "All Tasks Complete, Press Any Key to Exit\n\r"); // Nothing left // Upon maybe K2, the Kernel may be waiting at this point for user input, or other stuff for Processes to wake up. At this point, the Kernel should in theory spin
		// print the queue of all tasks, print by PID: state
		for (int i = 0; i < NUMPROCS; i++) {
			uart_printf(CONSOLE, "PID: %u, State: %u, Priority: %u\r\n", READY_QUEUE[i].pid, READY_QUEUE[i].time, READY_QUEUE[i].priority);
		}
	uart_getc(1);
	#endif
	EXIT();
}

int unblock_return(uint32_t interruptid, uint64_t ret){
	# if DEBUG == 4
		if (interruptid != CLOCKINTID)  uart_printf(CONSOLE, "KERNEL: unblock_return: interruptid = %u, ret = %u, len = %u\r\n", interruptid, ret, AWAIT_INTERRUPT[interruptid].len);
	# endif
	// AWAIT_INTERRUPT[eventType][AWAIT_INTERRUPT_LIST_LEN[eventType]] = currItem;	
	for (int i = 0; i < AWAIT_INTERRUPT[interruptid].len; i++) {
		
		struct state freed_state = AWAIT_INTERRUPT[interruptid].pid_ls[i];
		int p = PID - 1;
		(void)p; // Unused
		int p_free = freed_state.pid - 1;
		freed_state.time = READY;
		# if DEBUG == 4
			if (interruptid != CLOCKINTID) uart_printf(CONSOLE, "KERNEL: unblocked-process interruptid = %u, i = %u, pid = %u, priority = %u\r\n", 
						interruptid, i, 
						freed_state.pid, 
						freed_state.priority);
		# endif
		unblock(freed_state);
		PROCS[p_free].registervalues[0] = ret;
	}
	ret = AWAIT_INTERRUPT[interruptid].len;
	AWAIT_INTERRUPT[interruptid].len = 0;
	return ret;
}

void ExceptionASYNC(uint64_t esr_el1){
	(void)esr_el1;
    int p = PID - 1;
    
    
	// ExceptionASYNC(esr_el1);
	
	/*
	Begin(&PROCS[p].registervalues[0], PROCS[p].pcpointer, PROCS[p].stackpointer, PROCS[p].pstate); // found in asm.h
	*/

	// make switch case for the exception
	// uart_printf(CONSOLE, "ESR is %x\n\r", esr_el1); // DEBUG PRINT
    uint32_t interruptid = readInterruptId();
    #if DEBUG == 3
    // uart_printf(CONSOLE, "HandleASYNC: ESR is %x\n\r", esr_el1); // DEBUG PRINT
    // uart_printf(CONSOLE, "Asynchronouse SVC Call %x\n\r", interruptid); // DEBUG PRINT
    // uart_printf(CONSOLE, "ESR is %x\n\r", esr_el1); // DEBUG PRINT
    // uart_printf(CONSOLE, "PID = %u\n\r", PID); // DEBUG PRINT
    #endif
	setActiveInterrupt(interruptid);
	// make switch signal
	scrSchedule(PID, PROCS[p].priority);
	
	// if (CLOCKINTID != interruptid) uart_printf(CONSOLE, "NON CLOCK INTURRUPT\n\r");
	switch (interruptid) {
		case CLOCKINTID:
			// end the interrupt d
			// set the next timer
			// get the time
			set_timerC3(get_timerLO() + 10000);
			clear_timer_status();
			unblock_return(CLOCKINTID, 1);
			break;
		case UARTINTER:
			char return_val[8];
			// the first byte of the char is the type of interrupt given
			// the second byte of the char is the line number
			// the last byte of the char is the return value
			volatile uint32_t* RIS_CONSOLE = get_RIS(CONSOLE);
			volatile uint32_t* ICR_CONSOLE = get_ICR(CONSOLE);

			volatile uint32_t* RIS_MARKLIN = get_RIS(MARKLIN);
			volatile uint32_t* ICR_MARKLIN = get_ICR(MARKLIN);
			if((*RIS_CONSOLE) & (0x01 << CTSMIM)){
				// RXIC on the marklin
				return_val[0] = CTSMIM;
				return_val[1] = MARKLIN;
				if (get_CTS(MARKLIN) == 1) return_val[2] = 1; else return_val[2] = 0;
				*ICR_CONSOLE = (0x01 << CTSMIM);
			}else if((*RIS_MARKLIN) & (0x01 << TXIC)){
				// uart_printf(CONSOLE, "TXIC Interrupt ON MARKLIN\n\r");
				// TXIC on the marklin
				return_val[0] = TXIC;
				return_val[1] = MARKLIN;
				return_val[2] = -1;
				# if DEBUG == 4
					// print in green
				uart_printf(CONSOLE, "\033[32m");
				uart_printf(CONSOLE, "KERNEL: TXIC Interrupt ON MARKLIN  0x%x\r\n", *(uint64_t*)return_val);
				// print in white
				uart_printf(CONSOLE, "\033[37m");
				# endif
				*ICR_MARKLIN = (0x01 << TXIC);
			} else if((*RIS_MARKLIN) & (0x01 << RXIC)){
				// RXIC on the marklin
				return_val[0] = RXIC;
				return_val[1] = MARKLIN;
				return_val[2] = uart_getc_modified(MARKLIN);
				# if DEBUG == 4 
				// print in green
				uart_printf(CONSOLE, "\033[32m");
				uart_printf(CONSOLE, "KERNEL: RXIC Interrupt ON MARKLIN  0x%x\r\n", *(uint64_t*)return_val);
				// print in white
				uart_printf(CONSOLE, "\033[37m");
				# endif
				*ICR_MARKLIN = (0x01 << RXIC);
			} else if((*RIS_MARKLIN) & (0x01 << CTSMIM)){
				# if DEBUG == 4 
				// print in green 
				uart_printf(CONSOLE, "\033[32m");
				uart_printf(CONSOLE, "CTSMIM: Interrupt ON MARKLIN get_CTS(%u) = %u\n\r", MARKLIN, get_CTS(MARKLIN));
				// print in white
				uart_printf(CONSOLE, "\033[37m");
				# endif
				// RXIC on the marklin
				return_val[0] = CTSMIM;
				return_val[1] = MARKLIN;
				return_val[2] = get_CTS(MARKLIN);
				
				*ICR_MARKLIN = (0x01 << CTSMIM);
			}
			
			if (!unblock_return(interruptid, *(uint64_t*)return_val)){
				# if DEBUG == 4
					// print in red
					uart_printf(CONSOLE, "\033[31m");
					uart_printf(CONSOLE, "UART Interrupt: No one is waiting for this interrupt\n\r");
					// print in white
					uart_printf(CONSOLE, "\033[37m");
				# endif
				AWAIT_INTERRUPT[interruptid].event_q[AWAIT_INTERRUPT[interruptid].eventq_tail] = *(uint64_t*)return_val;
				AWAIT_INTERRUPT[interruptid].eventq_tail++;
				AWAIT_INTERRUPT[interruptid].eventq_tail %= NUMPROCS;
				AWAIT_INTERRUPT[interruptid].eventq_len++;
			}
			
			// INTERRUPT_CLEAR_ACTIVE_REGS(UARTINTER);
			break;
		default:
			# if DEBUG == 4
				uart_printf(CONSOLE, "Unknown Interrupt\n\r");
			# endif
			//scrSchedule(PID, PROCS[p].priority);
			break;
	}
	INTERRUPT_CLEAR_ACTIVE_REGS(interruptid);
	clear_GICC_EOIR(interruptid);

	// uart_printf(CONSOLE, "Exiting...\r\n");
    
}


// This is the function that is called when a syscall is made
// This is the contextswitch
//==================
void Handle(void* sp) // A helper function to pull some c variables into assembly
{
	// We just arrived here, there is stuff on the stack that I do not want to deal with
	int p = PID - 1;
	updateRunTimer();
	#if DEBUG == 3
	// uart_printf(CONSOLE, "Handle: Handling %x %x %x %x %x\r\n", sp, &PROCS[p].registervalues[0], &PROCS[p].pcpointer, &PROCS[p].stackpointer, &PROCS[p].pstate);
	#endif
	
	uint64_t esr_el1 = Save(sp, &PROCS[p].registervalues[0], &PROCS[p].pcpointer, &PROCS[p].stackpointer, &PROCS[p].pstate);
	// this is when the process is officially inturrupted.


	handlerExceptionHelper(esr_el1);
	Schedule();
	#if DEBUG_EXIT >= 1
		uart_printf(CONSOLE, "All Tasks Complete, Press Any Key to Exit\n\r"); // Nothing left // Upon maybe K2, the Kernel may be waiting at this point for user input, or other stuff for Processes to wake up. At this point, the Kernel should in theory spin
		// print the ready list (not the entire array)
		uart_printf(CONSOLE, "\r\nREADY LIST (size=%u):\r\n", READY_LIST.size);
		for (unsigned i = 0; i < READY_LIST.size; i++) {
			uart_printf(CONSOLE, "  [%u] PID: %u, Priority: %u\r\n", i, READY_LIST.items[i].pid, READY_LIST.items[i].priority);
		}
		uart_printf(CONSOLE, "\r\nAll PROCS states:\r\n");
		for (int i = 0; i < NUMPROCS; i++) {
			if (PROCS[i].pcpointer != NULL) {
				uart_printf(CONSOLE, "  PID %u: Priority: %u, PC: %p, SP: %p\r\n", 
				           PROCS[i].pid, PROCS[i].priority, PROCS[i].pcpointer, PROCS[i].stackpointer);
			}
		}
	uart_getc(1);
	EXIT();
	#endif
	
	
}

int8_t dead(int8_t p){
	return (PROCS[p].stackpointer == NULL && PROCS[p].pcpointer == NULL);
}
// Each parameter is now stored in the registers
// send_helper moved to layer2-messaging/messaging.c
// recieve_helper moved to layer2-messaging/messaging.c
// reply_helper moved to layer2-messaging/messaging.c

void handlerExceptionHelper(uint64_t esr_el1)
{
	#if DEBUG == 1
	// uart_printf(CONSOLE, "ESR is %x\n\r", esr_el1); // DEBUG PRINT
	#endif
	// PID is the currentlly running process
	int p = PID - 1;
	if (esr_el1 >> 24 == 86) { // an svc call has occured!
		int i = esr_el1 % 0x1000000;
		
		#if DEBUG == 1
		// print the running PID
		// // uart_printf(CONSOLE, "PID is %u ", PID); // DEBUG PRINT
		// // uart_printf(CONSOLE, "Case is %x\n\r", i); // DEBUG PRINT
		#endif
		
		switch (i) {
		
		case 0: // Exit
			Kill(p);
			break;
		case 1: // Yield
			scrSchedule(PID, PROCS[p].priority);
			break;
		case 2: // Create
			scrSchedule(PID, PROCS[p].priority);
			#if DEBUG_EXIT >= 1
			uart_printf(CONSOLE, "[DEBUG] After scrSchedule in Create, READY_LIST size=%u\r\n", READY_LIST.size);
			#endif
			int ret = KernelCreate((uint64_t)PROCS[p].registervalues[0], (void(*)())PROCS[p].registervalues[1], PID);
			#if DEBUG_EXIT >= 1
			uart_printf(CONSOLE, "[DEBUG] After KernelCreate, created TID=%d, READY_LIST size=%u\r\n", ret, READY_LIST.size);
			#endif
			PROCS[p].registervalues[0] = ret;
			break;
		case 3: // mytid
			scrSchedule(PID, PROCS[p].priority);
			PROCS[p].registervalues[0] = PROCS[p].pid;
			break;
		case 4: // parenttid
			// // uart_printf(CONSOLE, "PPID is %u\n\r", PROCS[p].parentpid); // DEBUG PRINT
		
			scrSchedule(PID, PROCS[p].priority);
			PROCS[p].registervalues[0] = PROCS[p].parentpid;
			break;
		case 5: // send blocks and unblocks other tasks
			// This unblocks the recieving task
			int dest_tid = PROCS[p].registervalues[0];
			int dest_p = dest_tid - 1;
			if (dead(dest_p)){
				# if DEBUG == 2
				// print apparentlly dest_tid is dead
				// uart_printf(CONSOLE, "%u is dead\r\n", dest_tid);
				# endif
				// The destination task does not exist
				scrSchedule(PID, PROCS[p].priority);
				PROCS[p].registervalues[0] = -1;
			} else if (PROCS[dest_p].queuesize >= QUEUESIZE){
				// the message failed to send due to the queue size being over QUEUESIZE
				# if DEBUG == 3
				uart_printf(CONSOLE, "Message failed to send due to the queue size being over QUEUESIZE, head = %u, tails = %u, PROCS[dest_tid].queuesize = %d\r\n", PROCS[dest_tid].waiting_recieve_head, PROCS[dest_tid].waiting_recieve_tail, PROCS[dest_tid].queuesize);
				# endif
	
				scrSchedule(PID, PROCS[p].priority);
				PROCS[p].registervalues[0] = -2;
			}
			else
			{
				// the destination does exist
				// Alright unblock the recieving task if it is blocked and expecting message
				// Blocks until a reply is generated
				// scrSchedule(PID, PROCS[p].priority, BLOCKED);
				block(PID, PROCS[p].priority);
				//PROCS[p].registervalues[0] = 
				send_helper();
			}
			// however there is another case in which the task unblocks
			break;
		case 6: // recieve blocks
			// scrSchedule(PID, PROCS[p].priority, BLOCKED);
			// There is a message in the mailbox
			#if DEBUG_EXIT >= 1
			uart_printf(CONSOLE, "[DEBUG] Receive called by PID=%u, blocking...\r\n", PID);
			#endif
			block(PID, PROCS[p].priority);
			#if DEBUG_EXIT >= 1
			uart_printf(CONSOLE, "[DEBUG] After block, READY_LIST size=%u\r\n", READY_LIST.size);
			#endif
			//PROCS[p].registervalues[0] = 
			recieve_helper(PID);
			
			break;
		case 7: // reply
			scrSchedule(PID, PROCS[p].priority);
			dest_tid = PROCS[p].registervalues[0];
			if(dead(dest_tid - 1)){
				# if DEBUG == 2
				// print apparentlly dest_tid is dead
				// uart_printf(CONSOLE, "Reply %u is dead\r\n", dest_tid);
				# endif
				//-1	tid is not the task id of an existing task.
				PROCS[p].registervalues[0] = -1;
			}
			else if(PROCS[dest_tid - 1].waiting_reply != 1){
				// the message is not recieved, thus reply is not possible
				// messsage is in three statges
				// sent, recieved, reply
				// not-sent 0 returns -2
				// sent 1 returns -3
				// recieved 2
				# if DEBUG == 2
				// print apparentlly dest_tid is dead
				// uart_printf(CONSOLE, "Reply %u is dead\r\n", dest_tid);
				# endif
				//-2	tid is not the task id of a reply-blocked task.
				PROCS[p].registervalues[0] = -2 - PROCS[dest_tid - 1].waiting_reply;
			}
			else 
				reply_helper();
			break;
		case 8: // MyPriority
			scrSchedule(PID, PROCS[p].priority);
			PROCS[p].registervalues[0] = PROCS[p].priority;
			break;
		case 9: // Create args
			scrSchedule(PID, PROCS[p].priority);
			uint64_t retd = KernelCreate((uint64_t)PROCS[p].registervalues[0], (void(*)())PROCS[p].registervalues[1], PID);
			
			if (PROCS[p].registervalues[2] > 0) {
				// theis is create with arguments
				// // uart_printf(CONSOLE, "Reg 3: %x\n\r", PROCS[p].registervalues[3
				// copy the first 8 registers from retptr to the registervalues
				for (int j = 0; j < 8; j++) {
					PROCS[retd - 1].registervalues[j] = ((int64_t *)PROCS[p].registervalues[3])[j];
				}
				// the rest of the parameters would be stored on the stack of the new process
				// remember the stack is a uint64_t array
				// store all the elements in args that cannot be stored in the registers into the stack
				if (PROCS[p].registervalues[2] > 8){
					int64_t *newsp = (int64_t *)PROCS[retd - 1].stackpointer;
					uint8_t stack_offset = PROCS[p].registervalues[2] - 8;
					newsp = newsp - (PROCS[p].registervalues[2] - 8); 
					if (stack_offset > 0){
						for (int j = 0; j < stack_offset; j++) {
							newsp[j] = ((int64_t *)PROCS[p].registervalues[3])[j + 8];
						}
						PROCS[retd - 1].stackpointer = (void *)newsp;
					}
				}
			}

			PROCS[p].registervalues[0] = retd;
			break;
		case 10: // get the interrupt
			// p is the current PID - 1
			uint64_t eventType = PROCS[p].registervalues[0];
			PROCS[p].registervalues[0] = -1;
			if (checkActiveInterrupt(eventType)){
				// check the interrupt queue, if the queue is empty then block the task
				if (AWAIT_INTERRUPT[eventType].eventq_len){
					// pop the queue
					#if DEBUG == 4
					if (eventType != CLOCKINTID) uart_printf(CONSOLE, "PID: %u, popped Interrupt %u event queue\r\n", PID, eventType);
					#endif
					scrSchedule(PID, PROCS[p].priority);
					PROCS[p].registervalues[0] = AWAIT_INTERRUPT[eventType].event_q[AWAIT_INTERRUPT[eventType].eventq_head];
					AWAIT_INTERRUPT[eventType].eventq_head++;
					AWAIT_INTERRUPT[eventType].eventq_head %= NUMPROCS;
					AWAIT_INTERRUPT[eventType].eventq_len--;

				} else{
					#if DEBUG == 4
					if (eventType != CLOCKINTID) uart_printf(CONSOLE, "PID: %u, Awaiting Interrupt %u\r\n", PID, eventType);
					#endif
					//scrSchedule(PID, PROCS[p].priority, BLOCKED);
					block(PID, PROCS[p].priority);
					struct state currItem = {PID, PROCS[p].priority, BLOCKED};
					AWAIT_INTERRUPT[eventType].pid_ls[AWAIT_INTERRUPT[eventType].len] = currItem;
					AWAIT_INTERRUPT[eventType].len = AWAIT_INTERRUPT[eventType].len + 1;
				}
			}else{
				scrSchedule(PID, PROCS[p].priority);
			}

			break;
		case 11: // get total time
			// uart_printf(CONSOLE, "Awaiting Interrupt %u\r\n", eventType);
			scrSchedule(PID, PROCS[p].priority);
			PROCS[p].registervalues[0] = PROCS[p].totaltime;
			break;
		case 12: // get kernel runtime
			scrSchedule(PID, PROCS[p].priority);
			PROCS[p].registervalues[0] = get_timerLO() - kernelStartTime;
			break;
		case 13: // MyProcessId - return process container id (threads in same process share memory)
			scrSchedule(PID, PROCS[p].priority);
			PROCS[p].registervalues[0] = PROCS[p].process_id;
			break;
		case 14: // GetProcessSharedMem - return base pointer of this process's shared memory
			scrSchedule(PID, PROCS[p].priority);
			{
				int pcid = PROCS[p].process_id;
				if (pcid >= 0 && pcid < NPROCESSES)
					PROCS[p].registervalues[0] = (uint64_t)(uintptr_t)PROCESS_CONTAINERS[pcid].shared_mem_base;
				else
					PROCS[p].registervalues[0] = 0;
			}
			break;

		default:
			scrSchedule(PID, PROCS[p].priority);
			# if DEBUG == 3
			uart_printf(CONSOLE, "Unknown SVC Call: %x\n\r", i); // DEBUG PRINT
			# endif
			break;
		}
	}
}

void Schedule()
{
	PID = scrPick();
	if ((int32_t)PID == -1) return;
	
	int p = PID - 1;
	// We need to reset the EL1 stack pointer as well
	
	#if DEBUG_EXIT >= 1
	uart_printf(CONSOLE, "[DEBUG] Schedule: About to Begin PID=%u, PC=%x, SP=%x, X0=%x\r\n", PID, (unsigned)(uint64_t)PROCS[p].pcpointer, (unsigned)(uint64_t)PROCS[p].stackpointer, (unsigned)PROCS[p].registervalues[0]);
	#endif
	
	#if DEBUG == 1
	uart_printf(CONSOLE, "Beginning pcpointer: %x stackpointer: %x registervalues: %x registervalues: %x %x %x %x %x %x %x %x\r\n", p, PROCS[p].pcpointer, PROCS[p].stackpointer, PROCS[p].registervalues[0], PROCS[p].registervalues[24], PROCS[p].registervalues[25], PROCS[p].registervalues[26], PROCS[p].registervalues[27], PROCS[p].registervalues[28], PROCS[p].registervalues[29], PROCS[p].registervalues[30]); // DEBUG
	// print all the register values
	uart_printf(CONSOLE, "PID is %u ", PID); // DEBUG PRINT
	uart_printf(CONSOLE, "PC is %x\n\r", PROCS[p].pcpointer); // DEBUG PRINT
	uart_printf(CONSOLE, "SP is %x\n\r", PROCS[p].stackpointer); // DEBUG PRINT
	// uart_getc(1); /// Spins to stop it from keep on running // DEBUG
	#endif
	// this begins the process, I would be keeping a timer here
	// Kernel need to keep track of the total runtime of the process
	// begibn 
	// WAKE UP For real
	PROCS[p].waketime = get_timerLO();

	Begin(&PROCS[p].registervalues[0], PROCS[p].pcpointer, PROCS[p].stackpointer, PROCS[p].pstate); // found in asm.h
}
// MINHEAP===================================== GET THE SMALLEST AVAILABLE PID


// if the heap is empty then increment the untouched_pid
// if the heap is not empty then pop the heap
struct MinHeap
{
	unsigned size;
	unsigned capacity;
	int harr[NUMPROCS];
};
void bubbleUp( struct MinHeap *h, int i)
{
	int parent = (i - 1) / 2;
	if (h->harr[parent] > h->harr[i])
	{
		int temp = h->harr[parent];
		h->harr[parent] = h->harr[i];
		h->harr[i] = temp;
		bubbleUp(h, parent);
	}
}
void bubbleDown( struct MinHeap *h, int i)
{
	unsigned int left = 2 * i + 1;
	unsigned int right = 2 * i + 2;
	int smallest = i;
	// if there is no child
	if ((unsigned int)left >= h->size && (unsigned int)right >= h->size)
		return;
	// if there is only left child
	if (left < h->size && (unsigned int)right >= h->size && h->harr[left] < h->harr[smallest])
		smallest = left;
	// no need to look for the case of only right child as it is a complete binary tree, no right child implies no left child implies no child
	// this is given there are two children
	if (left < h->size && h->harr[left] < h->harr[smallest])
		smallest = left;
	if (right < h->size && h->harr[right] < h->harr[smallest])
		smallest = right;
	if (smallest != i)
	{
		int temp = h->harr[smallest];
		h->harr[smallest] = h->harr[i];
		h->harr[i] = temp;
		bubbleDown(h, smallest);
	}
}
// add to the heap
void insertKey( struct MinHeap *h, int k)
{
	if (h->size == h->capacity)
	{
		// printf("\nOverflow: Could not insertKey\n");
		return;
	}
	h->harr[h->size] = k;
	bubbleUp(h, h->size);
	h->size++;
}
// check if the heap is empty
uint8_t isEmpty( struct MinHeap *h)
{
	return h->size == 0;
}
// pop the minimum element
int extractMin( struct MinHeap *h)
{
	if (h->size <= 0)
		return -1;
	if (h->size == 1)
	{
		h->size--;
		return h->harr[0];
	}
	int root = h->harr[0];
	h->harr[0] = h->harr[h->size - 1];
	h->size--;
	bubbleDown(h, 0);
	return root;
}


struct MinHeap pidheap;
int untouched_pid = 0;
int KernelCreate(uint8_t priority, void (*function)(), int parent)
{	
	
	int p = 0;
	if (isEmpty(&pidheap)) {
		p = untouched_pid;
		untouched_pid++;
	} else {
		p = extractMin(&pidheap);
	}
	while (PROCS[p].pcpointer != NULL) p++;
	if (PROCS[p].pcpointer == NULL) {
		// This slot is free - create thread (runnable unit)
		PROCS[p].pcpointer = function;
		PROCS[p].stackpointer = ((uint8_t*)STACKSTART) + (STACK_SIZE_PER_THREAD * (p + 1));
		// Maybe initialize PSTATE???
		// Registers initialized all to 0??
		PROCS[p].parentpid = parent;
		PROCS[p].process_id = (parent > 0) ? PROCS[parent - 1].process_id : 0; /* same process as parent, or process 0 */
		PROCS[p].priority = priority;
		PROCS[p].pid = p + 1;
		PROCS[p].pstate = 0;
		PROCS[p].waiting_reply = 0;
		PROCS[p].waiting_send = 0;
		PROCS[p].waiting_recieve_head = 0;
		PROCS[p].waiting_recieve_tail = 0;
		PROCS[p].queuesize = 0;
		PROCS[p].totaltime = 0;
		scrSchedule(p + 1, PROCS[p].priority);
		
		return p + 1;
	}
	
	return -2;
}
void Kill(int p) // p is the position of the process in the PROCS array
{
	PROCS[p].stackpointer = NULL;
	PROCS[p].pcpointer = NULL;
	insertKey(&pidheap, p); // gotta return the PID back to the original state
}

// ===== Layer 2 Functions: Send, Receive, Reply =====
// Moved to layer2-messaging/messaging.c
// See messaging.h for declarations

// The Following are EL0 commands (SVC stubs in layer0-assembly/syscalls.S)
int MyTid(void)
{
	return asm_svc_3();
}
int MyParentTid(void)
{
	return asm_svc_4();
}

int MyPriority(void)
{
	return asm_svc_8();
}

int Create(uint8_t priority, void (*function)(void))
{
	return asm_svc_2(priority, (void (*)(void))function);
}

int CreateArgs(uint8_t priority, void (*function)(), uint64_t argsno, uint64_t *args)
{
	(void)priority;
	(void)function;
	(void)argsno;
	(void)args;
	return asm_svc_9();
}
int AwaitEvent(int eventType)
{
	(void)eventType;
	return asm_svc_10();
}
int GetRuntime(void)
{
	return asm_svc_11();
}
int GetKernelRuntime(void)
{
	return asm_svc_12();
}
int MyProcessId(void)
{
	return asm_svc_13();
}
void *GetProcessSharedMem(void)
{
	return asm_svc_14();
}
int GetCurrentProcessId(void)
{
	if (PID <= 0 || PID > NUMPROCS)
		return 0;
	return PROCS[PID - 1].process_id;
}
// Stub implementation for Deregister - to be implemented when event registration is added
void Deregister() {
	// TODO: Clean up any event registrations before exit
	// Currently empty as event registration system is not yet implemented
}

// Why is exit SCV 0 
// The difference between an exit and a Yield is Exit do not return back to the priority READY_QUEUE where Yield returns the program back into the priority READY_QUEUE to be ran again. 
void Exit(void)
{
	Deregister();
	asm_svc_0();
}
void Yield(void)
{
	asm_svc_1();
}



