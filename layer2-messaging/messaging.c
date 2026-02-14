#include "messaging.h"
#include "../layer1-processes/syscall.h"
#include "layer0.h"
#include "../layer1-processes/rpi.h"
#include "../layer1-processes/util.h"

// External dependencies from layer1
extern uint32_t PID;
extern struct process PROCS[];
extern void block(int pid, uint8_t priority);
extern void unblock_ind(int pid, uint8_t priority);
extern void scrSchedule(int pid, uint8_t priority);
extern int dead(int p);

#define DEBUG 5

// Helper to get minimum of two integers
static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * send_helper - Handle Send syscall from kernel context
 * 
 * This function is called when a task executes Send(). It:
 * 1. Extracts parameters from the sender's register values
 * 2. Queues the message in the receiver's mailbox
 * 3. Unblocks the receiver if it was waiting for a message
 * 4. Blocks the sender until a reply is received
 */
void send_helper() {
    #if DEBUG == 2
    // uart_printf(CONSOLE, "===============\r\n Send Helper Called:\r\n");
    #endif
    
    // Extract sender's process info
    int p = PID - 1;
    int tid = (int)PROCS[p].registervalues[0];
    char *msg = (char *)PROCS[p].registervalues[1];
    uint64_t msglen = PROCS[p].registervalues[2];
    char *reply = (char *)PROCS[p].registervalues[3];
    uint64_t replylen = PROCS[p].registervalues[4];

    // Store the message details in sender's context
    PROCS[p].message_sent.tid = tid;
    PROCS[p].message_sent.msg = msg;
    PROCS[p].message_sent.msglen = msglen;
    PROCS[p].message_sent.reply = reply;
    PROCS[p].message_sent.replylen = replylen;
    PROCS[p].waiting_reply = 1;
    
    // Queue the message in receiver's mailbox
    int p_to = tid - 1;
    int tail = PROCS[p_to].waiting_recieve_tail;
    PROCS[p_to].message_recieved[tail].tid = PID;
    PROCS[p_to].message_recieved[tail].msg = msg;
    PROCS[p_to].message_recieved[tail].msglen = msglen;
    PROCS[p_to].message_recieved[tail].reply = reply;
    PROCS[p_to].message_recieved[tail].replylen = replylen;
    
    // Update receiver's mailbox pointers
    PROCS[p_to].waiting_recieve_tail++;
    PROCS[p_to].waiting_recieve_tail %= QUEUESIZE;
    PROCS[p_to].queuesize++;
    
    #if DEBUG == 2
    // uart_printf(CONSOLE, "===============\r\n Completed adding message to the queue:\r\n");
    // uart_printf(CONSOLE, "TID is %u\r\n", tid);
    // uart_printf(CONSOLE, "MSG is %s\r\n", msg);
    // uart_printf(CONSOLE, "MSGLEN is %u\r\n", msglen);
    // uart_printf(CONSOLE, "REPLY is %s\r\n", reply);
    // uart_printf(CONSOLE, "REPLYLEN is %u\r\n ============== \r\n", replylen);
    #endif
    
    // If receiver was waiting for a message, unblock it
    if (PROCS[p_to].waiting_send == 1) {
        #if DEBUG == 2
        // uart_printf(CONSOLE, "===============\r\n RECIEVE HELPER FOR %u by Send Helper Unblocked:\r\n", tid);
        #endif
        recieve_helper(tid);
    }
}

/**
 * recieve_helper - Handle Receive syscall from kernel context
 * 
 * This function is called when a task executes Receive(). It:
 * 1. Checks if there are messages in the mailbox
 * 2. If empty, blocks the task until a message arrives
 * 3. If messages exist, pops the oldest message and copies it to the receiver's buffer
 * 4. Unblocks the receiver task
 * 
 * @param PID Process ID of the receiving task
 */
void recieve_helper(int PID) {
    int p = PID - 1;
    int head = PROCS[p].waiting_recieve_head;
    int tail = PROCS[p].waiting_recieve_tail;
    
    #if DEBUG
    // uart_printf(CONSOLE, "=============== Recieve called by PID:%u\r\n", PID);
    // uart_printf(CONSOLE, "head is %u, tail is %u\r\n", head, tail);
    #endif
    
    // Extract receiver's parameters
    int *tid = (int *)PROCS[p].registervalues[0];  // Pointer to store sender TID
    char *msg = (char *)PROCS[p].registervalues[1]; // Pointer to message buffer
    int msglen = (int)PROCS[p].registervalues[2];   // Buffer size
    
    #if DEBUG == 2
    // uart_printf(CONSOLE, "p = %d\r\n", p);
    // uart_printf(CONSOLE, "TID is %x\r\n", tid);
    // uart_printf(CONSOLE, "MSG is %x\r\n", msg);
    // uart_printf(CONSOLE, "MSGLEN is %u\r\n ===============\r\n ", msglen);
    #endif
    
    // Check if mailbox is empty
    if (head == tail) {
        // Block the task - no messages available
        PROCS[p].waiting_send = 1;
        return;
    }
    
    PROCS[p].waiting_send = 0;
    
    #if DEBUG == 2
    // uart_printf(CONSOLE, "===============\r\n Proceeding with the Recieve Helper Called by %u:\r\n", PID);
    #endif
    
    // Pop message from head of queue
    uint64_t curread_message_length = PROCS[p].message_recieved[head].msglen;
    char *curread_msg = PROCS[p].message_recieved[head].msg;
    int sender_tid = PROCS[p].message_recieved[head].tid;
    
    // Store sender's TID
    *tid = sender_tid;
    
    // Copy message to receiver's buffer (truncate if necessary)
    msglen = min_int(msglen, curread_message_length);
    memcpy(msg, curread_msg, msglen);
    
    #if DEBUG == 2
    // uart_printf(CONSOLE, "head is %u, tail is %u\r\n", head, tail);
    // uart_printf(CONSOLE, "curread_msg is %s\r\n", curread_msg);
    // uart_printf(CONSOLE, "curread_message_length is %u\r\n", curread_message_length);
    // uart_printf(CONSOLE, "sender_tid TID is %u\r\n", sender_tid);
    // uart_printf(CONSOLE, "msg is %s\r\n", msg);
    // uart_printf(CONSOLE, "MSGLEN is %u\r\n ===============\r\n ", curread_message_length);
    #endif
    
    // Update mailbox head pointer
    PROCS[p].waiting_recieve_head++;
    if (PROCS[p].queuesize > 0) {
        PROCS[p].queuesize--;
    }
    PROCS[p].waiting_recieve_head %= QUEUESIZE;
    
    // Return message length to receiver
    PROCS[p].registervalues[0] = msglen;
    
    // Unblock the receiver task
    unblock_ind(PID, PROCS[p].priority);
}

/**
 * reply_helper - Handle Reply syscall from kernel context
 * 
 * This function is called when a task executes Reply(). It:
 * 1. Extracts the reply parameters
 * 2. Copies the reply into the sender's reply buffer
 * 3. Unblocks the original sender task (which was blocked in Send)
 */
void reply_helper() {
    #if DEBUG == 2
    // uart_printf(CONSOLE, "===============\r\n Reply Helper Called:\r\n");
    #endif
    
    // Extract reply parameters
    int p = PID - 1;
    int tid = PROCS[p].registervalues[0];          // Task to reply to
    char *reply = (char *)PROCS[p].registervalues[1];
    uint64_t replylen = PROCS[p].registervalues[2];
    
    // Get sender's reply buffer
    char *reply_buffer = PROCS[tid - 1].message_sent.reply;
    uint64_t reply_buffer_len = PROCS[tid - 1].message_sent.replylen;
    
    // Copy reply to sender's buffer (truncate if necessary)
    replylen = min_int(reply_buffer_len, replylen);
    memcpy(reply_buffer, reply, replylen);
    
    #if DEBUG == 2
    // uart_printf(CONSOLE, "reply_buffer length is %d\r\n", replylen);
    // uart_printf(CONSOLE, "TID is %u\r\n", tid);
    // uart_printf(CONSOLE, "REPLY is %s\r\n", reply);
    // uart_printf(CONSOLE, "REPLYLEN is %u\r\n ===============\r\n ", replylen);
    // uart_printf(CONSOLE, "replying to PID is %u, reply PID is %u\r\n", tid, PID);
    // uart_printf(CONSOLE, "reply_buffer is %x, *reply is %x\r\n", reply_buffer, reply);
    #endif
    
    // Unblock the original sender
    unblock_ind(tid, PROCS[tid - 1].priority);
    
    // Return reply length to both sender and replier
    PROCS[tid - 1].registervalues[0] = replylen;
    PROCS[tid - 1].waiting_reply = 0;
    PROCS[p].registervalues[0] = replylen;
}

// User-space API wrappers (these execute SVC instructions)

int Send(int tid, const char *msg, int msglen, char *reply, int replylen) {
    (void)tid; (void)msg; (void)msglen; (void)reply; (void)replylen;
    asm_svc_5();
    return 0;
}

int Receive(int *tid, char *msg, int msglen) {
    (void)tid; (void)msg; (void)msglen;
    asm_svc_6();
    return 0;
}

int Reply(int tid, void *reply, int replylen) {
    (void)tid; (void)reply; (void)replylen;
    asm_svc_7();
    return 0;
}
