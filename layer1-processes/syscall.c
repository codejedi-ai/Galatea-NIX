#include "syscall.h"
#include "processes.h"
#include "nameserver.h"
#include "asm.h"
#include "rpi.h"
#include "util.h"
#include "custstr.h"
# include "systimer.h"
#include "gic.h"
#define DEBUG 5
#define DEBUG_EXIT 1
# define READY 0
# define BLOCKED 1
// it is time to turn READY_QUEUE into a heap
// enqueing on a heap is O(log(n))
// first you 
uint32_t kernelStartTime = 0;
uint8_t NO_PARAMS = 0;
// HEAP IMPLEMENTATION

uint8_t compare_state(struct state a, struct state b)
{
	if (a.priority < b.priority)
		return 1;
	else if (a.priority > b.priority)
		return 2;
	else
	{
		if(a.time == -1)
			return 2;
		else if (b.time == -1)
			return 1;
		else if (a.time < b.time)
			return 1;
		else if (a.time > b.time)
			return 2;
		else
			return 0;
	}
}
void swap_state(struct state *a, struct state *b)
{
	struct state temp;
	temp.pid = a->pid;
	temp.priority = a->priority;
	temp.time = a->time;
	// *a = *b;
	a->pid = b->pid;
	a->priority = b->priority;
	a->time = b->time;
	// *b = temp;
	b->pid = temp.pid;
	b->priority = temp.priority;
	b->time = temp.time;
}
void _bubbleUp_state_heap( struct MinHeapState *h, int i)
{
	int parent = (i - 1) / 2;
	if (1 == compare_state(h->harr[i], h->harr[parent]))
	{
		swap_state(&h->harr[i], &h->harr[parent]);
		_bubbleUp_state_heap(h, parent);
	}
}
void bubbleDown_state_heap( struct MinHeapState *h, int i)
{
	int left = 2 * i + 1;
	int right = 2 * i + 2;
	int smallest = i;
	// if there is no child
	if (left >= h->size && right >= h->size)
		return;
	// if there is only left child
	if (left < h->size && right >= h->size && compare_state(h->harr[left], h->harr[smallest]) == 1)
		smallest = left;
	// no need to look for the case of only right child as it is a complete binary tree, no right child implies no left child implies no child
	// this is given there are two children
	if (left < h->size &&  compare_state(h->harr[left], h->harr[smallest]) == 1)
		smallest = left;
	if (right < h->size &&  compare_state(h->harr[right], h->harr[smallest]) == 1)
		smallest = right;
	if (smallest != i)
	{
		swap_state(&h->harr[i], &h->harr[smallest]);
		bubbleDown_state_heap(h, smallest);
	}
}
// add to the heap
void insertKey_state_heap( struct MinHeapState *h, struct state k)
{
	k.time = get_timerHI();
	k.time = k.time << 32;
	k.time += get_timerLO();
	if (h->size == h->capacity)
	{
		return;
	}
	h->harr[h->size] = k;
	_bubbleUp_state_heap(h, h->size);
	h->size++;
}
// check if the heap is empty
uint8_t isEmpty_state_heap( struct MinHeapState *h)
{
	return h->size == 0;
}
// pop the minimum element
struct state extractMin_state_heap( struct MinHeapState *h)
{
	if (h->size <= 0)
		return (struct state){-1,-1};
	if (h->size == 1)
	{
		h->size--;
		return h->harr[0];
	}
	/*
		int root = h->harr[0];
	h->harr[0] = h->harr[h->size - 1];
	h->size--;
	*/
	struct state root = h->harr[0];
	h->harr[0] = h->harr[h->size - 1];
	h->size--;
	bubbleDown_state_heap(h, 0);
	return root;
}

//HEAP IMPLEMENTATION END

// scrSchedule(pid, priority)
void scrSchedule(int pid, uint64_t priority)
{
	
	struct state currItem = {pid, priority, get_timerHI() << 32 + get_timerLO()};
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
	// put the process into the heap
	insertKey_state_heap(&READY_HEAP, currItem);
	return 0;
}

// scrSchedule(pid, priority)
int unblock_ind(int pid, uint64_t priority)
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
	scrSchedule(pid, priority);
	BLOCKED_LIST[pid - 1].pid = 0;
	return 0;
}
/*
scrSchedule(PID, PROCS[p].priority, BLOCKED);
block(PID, PROCS[p].priority);
					*/
void block(int pid, uint64_t priority){
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
	uint64_t priority = currItem.priority;
	int ready = currItem.time;
	// uart_printf(CONSOLE, "unblock: pid = %u priority = %u ready =%u\r\n", pid, priority);
	
	unblock_ind(pid, priority);
}
int scrPick()
{
	int pid = -1;
	int bump = 0;
	/*
	struct state emptyItem = {0, 0, 0};
	
	for (int i = 0; i < NUMPROCS; i++) {
		if (bump) {
			READY_QUEUE[i - 1] = READY_QUEUE[i];
		}
		else if (READY_QUEUE[i].pid > 0 && READY_QUEUE[i].time == READY) {
			pid = READY_QUEUE[i].pid;
			bump = 1;
		}
		
	}
	if (bump) {READY_QUEUE[NUMPROCS - 1] = emptyItem;}
	*/
	if (isEmpty_state_heap(&READY_HEAP)) return -1;
	struct state currItem = extractMin_state_heap(&READY_HEAP);
	pid = currItem.pid;
	return pid;
}
void debugPrint(char *str){
	#if DEBUG == 4
	uart_printf(CONSOLE, str);
	#endif
}
// init the initial variables the system goes by
void InitSys(void* reg)
{	// For some reason, normal init to 0 just.. doesn't work?
	kernelStartTime = get_timerLO();
	READY_HEAP.size = 0;
	READY_HEAP.capacity = NUMPROCS;
	READY_HEAP.harr = READY_QUEUE;
	STACKSTART = reg;
	PID = 0;
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
	for (int idx = 0; idx < NUMPROCS; idx++) {
		PROCS[idx].stackpointer = NULL;
		PROCS[idx].pcpointer = NULL;
		PROCS[idx].pstate = 0;
		PROCS[idx].parentpid = 0;
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
	return 0;
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
			resetCS(3);
			unblock_return(CLOCKINTID, 1);
			break;
		case UARTINTER:
			char return_val[8];
			// the first byte of the char is the type of interrupt given
			// the second byte of the char is the line number
			// the last byte of the char is the return value
			uint32_t* RIS_CONSOLE = get_RIS(CONSOLE);
			uint32_t* ICR_CONSOLE = get_ICR(CONSOLE);

			uint32_t* RIS_MARKLIN = get_RIS(MARKLIN);
			uint32_t* ICR_MARKLIN = get_ICR(MARKLIN);
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
		// print the queue of all tasks, print by PID: state
		for (int i = 0; i < NUMPROCS; i++) {
			uart_printf(CONSOLE, "PID: %u, State: %u, Priority: %u\r\n", READY_QUEUE[i].pid, READY_QUEUE[i].time, READY_QUEUE[i].priority);
		}
	uart_getc(1);
	EXIT();
	#endif
	
	
}

int8_t dead(int8_t p){
	return (PROCS[p].stackpointer == NULL && PROCS[p].pcpointer == NULL);
}
// Each parameter is now stored in the registers
void send_helper(){
	# if DEBUG == 2
	// print the function called
	// uart_printf(CONSOLE, "===============\r\n Send Helper Called:\r\n");
	# endif
	// Debug
	int p = PID - 1;
	int tid = PROCS[p].registervalues[0];
	char *msg = PROCS[p].registervalues[1];
	uint64_t msglen = PROCS[p].registervalues[2];
	char *reply = PROCS[p].registervalues[3];
	uint64_t replylen = PROCS[p].registervalues[4];

	// This puts the message into the messageDS of the target task
	PROCS[p].message_sent.tid = tid;
	PROCS[p].message_sent.msg = msg;
	PROCS[p].message_sent.msglen = msglen;
	PROCS[p].message_sent.reply = reply;
	PROCS[p].message_sent.replylen = replylen;
	PROCS[p].waiting_reply = 1;
	// This is the target task, if it is waiting_send then we need ot remove the waiting send and unblock the task
	int p_to = tid - 1;
	int tail = PROCS[p_to].waiting_recieve_tail;
	PROCS[p_to].message_recieved[tail].tid = PID; // This is the tricky part for the recieved it should be the sender's pid
	PROCS[p_to].message_recieved[tail].msg = msg;
	PROCS[p_to].message_recieved[tail].msglen = msglen;
	PROCS[p_to].message_recieved[tail].reply = reply;
	PROCS[p_to].message_recieved[tail].replylen = replylen;
	// // uart_printf(CONSOLE, "reply addr is %x\r\n", reply);
	PROCS[p_to].waiting_recieve_tail++;
	PROCS[p_to].waiting_recieve_tail %= QUEUESIZE;
	PROCS[p_to].queuesize++;
	
	# if DEBUG == 2
	// print the function called
	// uart_printf(CONSOLE, "===============\r\n Completed adding message to the queue:\r\n");
	// print all the params
	// uart_printf(CONSOLE, "TID is %u\r\n", tid);
	// uart_printf(CONSOLE, "MSG is %s\r\n", msg);
	// uart_printf(CONSOLE, "MSGLEN is %u\r\n", msglen);
	// uart_printf(CONSOLE, "REPLY is %s\r\n", reply);
	// uart_printf(CONSOLE, "REPLYLEN is %u\r\n ============== \r\n", replylen);
	# endif
	if (PROCS[p_to].waiting_send == 1){
		// The task is waiting for a message
		// unblock the task
		# if DEBUG == 2
		// print therefore unblocked
		// uart_printf(CONSOLE, "===============\r\n RECIEVE HELPER FOR %u by Send Helper Unblocked:\r\n", tid);
		# endif

		recieve_helper(tid);
	}
	// At this point we need to wake up the message processing task
	
}
// It assumes that the messageDS is not empty
// recieve takes a message from the mailbox and returns the message inplace
void recieve_helper(int PID){
	int p = PID - 1;
	int head = PROCS[p].waiting_recieve_head;
	int tail = PROCS[p].waiting_recieve_tail;
	# if DEBUG
	// print PID
	// uart_printf(CONSOLE, "=============== Recieve called by PID:%u\r\n", PID);
	// uart_printf(CONSOLE, "head is %u, tail is %u\r\n", head, tail);
	# endif
	
	int *tid =  PROCS[p].registervalues[0]; // This is a memory address for the TID
	char *msg = PROCS[p].registervalues[1]; // this is another memory address for the message
	int msglen = PROCS[p].registervalues[2];
	# if DEBUG == 2
	// print therefore blocked
	
	// print the value of the tid poitnter msg pointer and the msglen pointer
	// uart_printf(CONSOLE, "p = %d\r\n", p);
	// uart_printf(CONSOLE, "TID is %x\r\n", tid);
	// uart_printf(CONSOLE, "MSG is %x\r\n", msg);
	// uart_printf(CONSOLE, "MSGLEN is %u\r\n ===============\r\n ", msglen);

	# endif
	if (head == tail){
		// // uart_printf(CONSOLE, "===============\r\n Recieve Helper Blocked:\r\n");
		// The messageDS is empty
		// Block the task
		// it is replying to a not reply blocked task

		PROCS[p].waiting_send = 1;
		return;
	}
	PROCS[p].waiting_send = 0;
	// unblock the task I really do not know how to unblock the task. If it was just blankly unblocked it would just return 
	// and keep running with no message
	
	// HOWEVER THE TASK IS STILL BLOCKED ONE MUST REPLY TO THE MESSAGE
	# if DEBUG == 2
	// Print the function called
	// uart_printf(CONSOLE, "===============\r\n Proceeding with the Recieve Helper Called by %u:\r\n", PID);
	# endif
	// Those are the returning variables. The memories needed to be written when the recieve function returns
	// THE RECIEVING TASK IS READING INTO THE MEMORY BUFFER OF THE SENDING TASK
	// *tid is a pointer to the memory address of the TID




	
	// First need to access the mailbox
	// The mailbox is the messageDS of the process
	// The mailbox is a circular READY_QUEUE

	uint64_t  curread_message_length = PROCS[p].message_recieved[head].msglen;
	char *curread_msg = PROCS[p].message_recieved[head].msg;
	int sender_tid = PROCS[p].message_recieved[head].tid;
	*tid = sender_tid;
	# if DEBUG == 2 
		// strflush(curread_msg, curread_message_length); 
	# endif
	// msg is the destination curread_msg is the source
	// msglen = cust_strcpy(msg, msglen - 1, curread_msg, curread_message_length - 1) + 1;
	msglen = min(msglen, curread_message_length);
	memcpy(msg, curread_msg, msglen);
	
	# if DEBUG == 2 
		// strflush(msg, msglen); 
		// uart_printf(CONSOLE, "recieve_buffer length is %d\r\n", msglen);
	# endif
	// DEBUG
	# if DEBUG == 2
	// Print the recieved message
	// uart_printf(CONSOLE, "head is %u, tail is %u\r\n", head, tail);
	// uart_printf(CONSOLE, "curread_msg is %s\r\n", curread_msg);
	// uart_printf(CONSOLE, "curread_message_length is %u\r\n", curread_message_length);
	// uart_printf(CONSOLE, "sender_tid TID is %u\r\n", sender_tid);
	// uart_printf(CONSOLE, "msg is %s\r\n", msg);
	// uart_printf(CONSOLE, "MSGLEN is %u\r\n ===============\r\n ", curread_message_length);
	# endif
	
	// update the head

	# if DEBUG == 2
	// Print the recieved message
	// uart_printf(CONSOLE, "TID is %u\r\n", sender_tid);
	// uart_printf(CONSOLE, "curread_msg is %s\r\n", curread_msg);
	// uart_printf(CONSOLE, "MSGLEN is %u\r\n ===============\r\n ", msglen);
	# endif
	// p is the current process, the process that is recieving
	PROCS[p].waiting_recieve_head++;
	if(PROCS[p].queuesize > 0){
		PROCS[p].queuesize--;
	}
	PROCS[p].waiting_recieve_head %= QUEUESIZE;
	PROCS[p].registervalues[0] = msglen;
	unblock_ind(PID, PROCS[p].priority);
	// return msglen;
	
	// this is the sender process. The sender is ready for a reply
}
void reply_helper(){
	# if DEBUG == 2
	// uart_printf(CONSOLE, "===============\r\n Reply Helper Called:\r\n");
	# endif
	// replies to the PID
	int p = PID - 1;
	int tid = PROCS[p].registervalues[0];
	char *reply = (char *)PROCS[p].registervalues[1];
	uint64_t replylen = PROCS[p].registervalues[2];
	char *reply_buffer = PROCS[tid - 1].message_sent.reply;
	uint64_t reply_buffer_len = PROCS[tid - 1].message_sent.replylen;
	// Have the kernel copy the reply into the messageDS of the target task

	// PROCS[tid - 1].message_sent.reply[replylen] = 0;


	// // uart_printf(CONSOLE, "*reply is %c\r\n", *reply);
	replylen = min(reply_buffer_len, replylen);
	memcpy(reply_buffer, reply, replylen);
	// replylen = cust_strcpy(reply_buffer, reply_buffer_len - 1, reply, replylen - 1) + 1;

	
	# if DEBUG == 2
	// uart_printf(CONSOLE, "reply_buffer length is %d\r\n", replylen);
	// Print the recieved message
	// uart_printf(CONSOLE, "TID is %u\r\n", tid);
	// uart_printf(CONSOLE, "REPLY is %s\r\n", reply);
	// uart_printf(CONSOLE, "REPLYLEN is %u\r\n ===============\r\n ", replylen);
	// uart_printf(CONSOLE, "replying to PID is %u, reply PID is %u\r\n", tid , PID );
	// uart_printf(CONSOLE, "reply_buffer is %x, *reply is %x\r\n", reply_buffer, reply);
	# endif
	// 
	// now unblock the target task
	unblock_ind(tid, PROCS[tid - 1].priority);
	// return a reply length for the send function
	PROCS[tid - 1].registervalues[0] = replylen;
	PROCS[tid - 1].waiting_reply = 0;
	// return for the reply function
	PROCS[p].registervalues[0] = replylen;
	
}

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
			int ret = KernelCreate(PROCS[p].registervalues[0], PROCS[p].registervalues[1], PID);
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
			block(PID, PROCS[p].priority);
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
			uint64_t retd = KernelCreate(PROCS[p].registervalues[0], PROCS[p].registervalues[1], PID);
			
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
						PROCS[retd - 1].stackpointer = (int64_t)newsp;
					}
				}
			}
		
			PROCS[p].registervalues[0] = ret;
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
	if (PID == -1) return 0;
	
	int p = PID - 1;
	// We need to reset the EL1 stack pointer as well
	
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
	return 0;
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
	int left = 2 * i + 1;
	int right = 2 * i + 2;
	int smallest = i;
	// if there is no child
	if (left >= h->size && right >= h->size)
		return;
	// if there is only left child
	if (left < h->size && right >= h->size && h->harr[left] < h->harr[smallest])
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
int KernelCreate(uint64_t priority, void (*function)(), int parent)
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
		// This PID is currently not taken
		PROCS[p].pcpointer = function;
		PROCS[p].stackpointer = ((uint8_t*)STACKSTART) + (0x10000 * (p + 1)); // We need to check this
		// Maybe initialize PSTATE???
		// Registers initialized all to 0??
		PROCS[p].parentpid = parent; // MAYBE CHANGE THIS
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
// k2 send receive reply
// k2 send
// Version 1 implementation would have the task directlly tell the kernel
// to put a specific message in the messageDS of the targeted task. 
int Send(int tid, const char *msg, int msglen, char *reply, int replylen){
	asm("svc 5");
	return;
}

int Receive(int *tid, char *msg, int msglen){
	asm("svc 6");
	return;
}
int Reply( int tid, void *reply, int replylen ){
	asm("svc 7");
	return;
}
int MyPriority(){
	asm("svc 8");
	return;
}
// helper functions 
// mailbox is meant to get the messageDS array of the process. 
// It would work exactlly like a mailbox.

// The Following are EL0 commands
int MyTid()
{
	asm("svc 3");
	return;
}
int MyParentTid()
{
	asm("svc 4");
	return;
}

int Create(uint64_t priority, void (*function)()) { // Returns to the Kernel, then calls KernelCreate
	asm("svc 2"); // The Kernel needs to put the pid in x0
	return;
}

int CreateArgs(uint64_t priority, void (*function)(), uint64_t argsno, uint64_t *args) { // Returns to the Kernel, then calls KernelCreate
	asm("svc 9"); // The Kernel needs to put the pid in x0
	return;
}
int AwaitEvent(int eventType){ // Returns to the Kernel, then calls KernelCreate
	asm("svc 10"); // The Kernel needs to put the pid in x0
	return;
}
int GetRuntime(){ // Returns to the Kernel, then calls KernelCreate
	asm("svc 11"); // The Kernel needs to put the pid in x0
	return;
}
int GetKernelRuntime(){ // Returns to the Kernel, then calls KernelCreate
	asm("svc 12"); // The Kernel needs to put the pid in x0
	return;
}
// Why is exit SCV 0 
// The difference between an exit and a Yield is Exit do not return back to the priority READY_QUEUE where Yield returns the program back into the priority READY_QUEUE to be ran again. 
void Exit()
{
	Deregister();
	asm("svc 0");
	return;
}
void Yield()
{
	asm("svc 1");
	return;
}



