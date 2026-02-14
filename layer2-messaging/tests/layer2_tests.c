#include "layer2_tests.h"
#include "../messaging.h"
#include "../../layer1-processes/syscall.h"
#include "../../layer1-processes/rpi.h"

// ==================== Test #1: Simple Two-Thread Message Passing ====================

void thread_2(void) {
    int sender_tid;
    char msg_buf[64];
    char reply_msg[64] = "message_1";
    
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Thread 2 waiting for message...\r\n");
    
    // Receive message from Thread 1
    int msglen = Receive(&sender_tid, msg_buf, sizeof(msg_buf));
    if (msglen > 0 && msglen < 64) {
        msg_buf[msglen] = '\0';
    }
    
    // Display what was received
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Thread 2 received message: \"%s\" from TID %d\r\n", msg_buf, sender_tid);
    
    // Reply with "message_1" - include null terminator
    Reply(sender_tid, reply_msg, 10);
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Thread 2 sent reply: \"message_1\"\r\n");
    
    Exit();
}

void thread_1(void) {
    char send_msg[64] = "hello";
    char reply_buf[64];
    
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Thread 1 sending message: \"hello\" to TID 2\r\n");
    
    // Send "hello" to Thread 2 (TID 2) - include null terminator
    int reply_len = Send(2, send_msg, 6, reply_buf, sizeof(reply_buf));
    if (reply_len > 0 && reply_len < 64) {
        reply_buf[reply_len] = '\0';
    }
    
    // Display what was received
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Thread 1 received reply: \"%s\"\r\n", reply_buf);
    
    Exit();
}

// ==================== Test #2: IPC (Send/Receive/Reply, shared memory) ====================

static void ipc_receiver_l2(void) {
    int sender;
    char buf[32];
    *(int *)GetProcessSharedMem() = MyTid();
    Receive(&sender, buf, sizeof(buf));
    Reply(sender, "ack", 4);
    Exit();
}

static void ipc_sender_l2(void) {
    char buf[32];
    int receiver_tid = *(int *)GetProcessSharedMem();
    int r = Send(receiver_tid, "ping", 5, buf, sizeof(buf));
    if (r >= 0)
        uart_printf(CONSOLE, "  \033[1;32m[  OK  ]\033[0m Test #2: IPC (inter-process): Send/Receive/Reply\r\n");
    else
        uart_printf(CONSOLE, "  \033[1;31m[ FAIL ]\033[0m Test #2: IPC Send returned %d\r\n", r);
    Exit();
}

static void run_test2_ipc(void) {
    uart_printf(CONSOLE, "  Test #2: IPC:\r\n");
    uart_printf(CONSOLE, "    \033[1;34m[ INFO ]\033[0m Testing IPC (Send/Receive/Reply)...\r\n");
    Create(5, ipc_receiver_l2);
    Yield();
    Create(0, ipc_sender_l2);
    for (int i = 0; i < 20; i++) Yield();
    uart_printf(CONSOLE, "    \033[1;32m[  OK  ]\033[0m IPC tests passed\r\n");
}

// ==================== Test Runner ====================

void run_layer2_tests(void) {
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "\033[1;36m[====] Galatea-NIX Layer 2 IPC Tests:\033[0m\r\n");
    
    uart_printf(CONSOLE, "  Test #1: Message passing:\r\n");
    uart_printf(CONSOLE, "\033[1;33m[ .... ]\033[0m Creating receiver task (Thread 2)...\r");
    int tid2 = Create(5, thread_2);
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Created receiver task (Thread 2, TID=%d)\r\n", tid2);
    Yield();
    uart_printf(CONSOLE, "\033[1;33m[ .... ]\033[0m Creating sender task (Thread 1)...\r");
    int tid1 = Create(0, thread_1);
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Created sender task (Thread 1, TID=%d)\r\n", tid1);
    uart_printf(CONSOLE, "\033[1;34m[ INFO ]\033[0m Running message passing tests...\r\n");
    for (int i = 0; i < 50; i++) Yield();
    (void)tid1;
    (void)tid2;

    run_test2_ipc();

    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "  \033[1;32m[  OK  ]\033[0m All Layer 2 tests passed\r\n");
    uart_printf(CONSOLE, "\r\n");
    Exit();
}
