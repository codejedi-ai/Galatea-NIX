#ifndef _syscall_h_
#define _syscall_h_ 1
#include "config.h"
#include "asm.h"

// Forward declarations for kernel-internal data structures
// (actual definitions are in syscall.c)

// static const int NUMPROCS = 20; // Deprecated

void InitSys(void* reg);
void Handle(void* sp);
void Exception(uint64_t esr_el1);

void Kill(int p);
int KernelCreate(uint8_t priority, void (*function)(), int parent);
int Create(uint8_t priority, void (*function)());
int CreateArgs(uint8_t priority, void (*function)(), uint64_t argsno, uint64_t *args);
void Schedule();

int MyTid();
int MyPriority();

int MyParentTid();

void Yield();
void Exit(); 

// ===== Layer 2: Message Passing (IPC) =====
// Implementation in ../layer2-messaging/messaging.c
// These functions enable inter-process communication via Send-Receive-Reply protocol
int Send(int tid, const char *msg, int msglen, char *reply, int replylen);
int Receive(int *tid, char *msg, int msglen);
int Reply( int tid, void *reply, int replylen );

// Message structure for IPC
struct message{
    int tid; // to which task
    char *msg;
    uint64_t msglen;
    char *reply;
    uint64_t replylen;
};

/*
 * Process = memory management only. Owns a shared memory region (base + size).
 * Threads within a process share the same memory space (this region).
 */
struct process_container {
	int pid;                  /* process id */
	void *shared_mem_base;    /* shared memory: all threads in this process use this */
	uint32_t shared_mem_size;
};

/*
 * Thread = runnable unit (stack, PC, registers). process_id = which process;
 * threads within a process share that process's memory space.
 */
struct process {
	void *stackpointer;
	void (*pcpointer)();
	uint32_t pstate;
	int parentpid;
	int pid;                  /* thread id (TID) - used in scheduling, MyTid() */
	int process_id;           /* which process container this thread belongs to */
	uint8_t priority;
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
	uint8_t priority;
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

void scrSchedule(int pid, uint8_t priority);
int scrPick();
void HandleASYNC(void* sp);
void ExceptionASYNC(uint64_t esr_el1);
int AwaitEvent(int eventType);
int GetRuntime();
int GetKernelRuntime();
int MyProcessId(void);
void *GetProcessSharedMem(void);
/* Kernel-only: current process container id (for alloc/free). Use when in kernel context. */
int GetCurrentProcessId(void);
#endif
