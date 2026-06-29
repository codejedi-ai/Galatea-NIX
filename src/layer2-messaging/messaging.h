#ifndef MESSAGING_H
#define MESSAGING_H

#include <stdint.h>

// Note: struct message is defined in syscall.h as part of the process control block

// Layer 2 Message Passing API - User-space wrappers
// These call into the kernel via SVC instructions

/**
 * Send a message to another task and wait for reply
 * @param tid Target task ID
 * @param msg Message buffer to send
 * @param msglen Length of message
 * @param reply Buffer to receive reply
 * @param replylen Size of reply buffer
 * @return Reply length on success, -1 if task doesn't exist, -2 if queue full
 */
int Send(int tid, const char *msg, int msglen, char *reply, int replylen);

/**
 * Receive a message from any sender
 * @param tid Pointer to store sender's task ID
 * @param msg Buffer to store received message
 * @param msglen Size of receive buffer
 * @return Message length on success
 */
int Receive(int *tid, char *msg, int msglen);

/**
 * Reply to a task that sent a message
 * @param tid Task ID to reply to
 * @param reply Reply message buffer
 * @param replylen Length of reply
 * @return Reply length on success
 */
int Reply(int tid, const void *reply, int replylen);

// Kernel-side message handling helpers
// These are called by the syscall handler

/**
 * Handle Send syscall - copy message to receiver's queue
 * Called from kernel context when Send() is invoked
 */
void send_helper(void);

/**
 * Handle Receive syscall - pop message from mailbox
 * @param PID Process ID of receiving task
 * Called from kernel context when Receive() is invoked
 */
void recieve_helper(int PID);

/**
 * Handle Reply syscall - copy reply to sender's buffer
 * Called from kernel context when Reply() is invoked
 */
void reply_helper(void);

#endif // MESSAGING_H
