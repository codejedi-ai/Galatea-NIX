#ifndef _syscall_h_
#define _syscall_h_ 1
#include "asm.h"
#define QUEUESIZE 255
#define NUMPROCS 20
#define MAXINT 2147483647
#define MININT -2147483648
#define MAXEVENT 1025
#define CLOCKINTID 99
#define UARTINTER 153

# define RITC 6
# define TXIC 5
# define RXIC 4
# define CTSMIM 1
static void *STACKSTART;
// This is the PID of the currentlly running process
static uint32_t PID = 0;




// static const int NUMPROCS = 20; // Deprecated

void InitSys(void* reg);
void Handle();
void Exception(uint64_t esr_el1);

void Kill(int p);
int KernelCreate(uint64_t priority, void (*function)(), int parent);
int Create(uint64_t priority, void (*function)());
int CreateArgs(uint64_t priority, void (*function)(), uint64_t argsno, uint64_t *args);
void Schedule();

int MyTid();
int MyPriority();

int MyParentTid();

void Yield();
void Exit(); 

// This is where K2 starts
int Send(int tid, const char *msg, int msglen, char *reply, int replylen);
int Receive(int *tid, char *msg, int msglen);
int Reply( int tid, void *reply, int replylen );
struct message{
    int tid; // to which task
    char *msg;
    uint64_t msglen;
    char *reply;
    uint64_t replylen;
};

struct process {
	void *stackpointer;
	void (*pcpointer)();
	uint32_t pstate;
	int parentpid;
	int pid;
	uint64_t priority;
	uint64_t registervalues[31];
	// define an array of messages such would be held in memory for each process.
	// A kernel call is needed to get the message array for the process.
	struct message message_sent;
	struct message message_recieved[QUEUESIZE];
	uint64_t waiting_recieve_head;
	uint64_t waiting_recieve_tail;
	uint64_t queuesize;
	uint64_t waiting_recieve;
	uint64_t waiting_reply;
	uint64_t waiting_send;

	uint32_t totaltime;
	uint32_t waketime;
};
struct state {
	int pid;
	uint64_t priority;
    uint64_t time;
};
struct interrupt {
	struct state pid_ls[NUMPROCS]; // this is the list that is to be unblocked when interrupt happened
	uint64_t event_q[NUMPROCS];
	int len;
	int eventq_len, eventq_head, eventq_tail;
};

struct MinHeapState
{
	unsigned size;
	unsigned capacity;
	struct state *harr;
};
static struct process PROCS[NUMPROCS];
static struct state READY_QUEUE[NUMPROCS];
static struct state BLOCKED_LIST[NUMPROCS];
static struct MinHeapState READY_HEAP;
static struct interrupt AWAIT_INTERRUPT[MAXEVENT];
void scrSchedule(int pid, uint64_t priority);
int scrPick();
void HandleASYNC(void* sp);
void ExceptionASYNC(uint64_t esr_el1);
int AwaitEvent(int eventType);
int GetRuntime();
int GetKernelRuntime();
#endif
