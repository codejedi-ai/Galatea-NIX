# Layer 2 Messaging Tests

Comprehensive tests for the inter-process communication (IPC) system using Send-Receive-Reply protocol.

## Test Scenarios

### 1. Echo Server Test (`test_message_passing`)
Tests basic Send-Receive-Reply functionality with multiple clients.

**Components:**
- **echo_server**: Receives messages from 3 clients, echoes them back
- **echo_client_a/b/c**: Three clients that send messages and wait for replies

**What it tests:**
- Basic message sending
- Message receiving and queuing
- Reply delivery
- Multiple clients communicating with one server
- Message ordering in the receive queue

**Expected output:**
```
[ECHO_SERVER] Started, TID=2
[CLIENT_A] Sending to server TID 2
[ECHO_SERVER] Received from TID 3: 'Hello from Client A'
[ECHO_SERVER] Replied to TID 3
[CLIENT_A] Got reply: 'Hello from Client A'
...
```

### 2. Calculator Server Test
Demonstrates a simple request-response pattern for compute tasks.

**Components:**
- **calculator_server**: Receives arithmetic operation requests
- **calculator_client**: Sends calculation requests

**What it tests:**
- Request-response pattern
- Message format handling
- Reply content verification

### 3. Stress Test (`test_ipc_mechanisms`)
Tests system behavior under heavy IPC load.

**Components:**
- **stress_server**: Handles 10 messages from multiple clients
- **stress_client**: Each client sends 3 rapid messages (3 clients total)

**What it tests:**
- Message queue handling with multiple concurrent senders
- Proper blocking/unblocking of tasks
- System stability under load
- Queue overflow prevention (QUEUESIZE limit)

**Expected behavior:**
- Server should handle all 9 messages (3 clients × 3 messages)
- Messages should be processed in FIFO order
- No messages should be lost
- All clients should successfully complete

## Running the Tests

The tests are automatically run when the kernel boots with `run_layer2_tests()` set as the first user task in `main.c`.

To run:
```bash
make clean && make
./run_qemu.sh
```

## Test Coverage

✅ **Send syscall**
- Sending messages to valid tasks
- Blocking sender until reply received
- Message queue management

✅ **Receive syscall**
- Receiving messages from mailbox
- Blocking when no messages available
- Unblocking when message arrives
- FIFO message ordering

✅ **Reply syscall**
- Replying to blocked senders
- Unblocking sender upon reply
- Reply data copying

✅ **Error cases**
- Full message queue (returns -2)
- Dead task (returns -1)
- Multiple concurrent senders

## Success Criteria

Tests pass if:
1. All messages are successfully delivered
2. All replies are received by clients
3. No deadlocks occur
4. Message ordering is preserved
5. All tasks complete and exit cleanly
6. No kernel panics or crashes

## Architecture Notes

The messaging system implements a **synchronous message passing** model:
- **Send**: Blocks sender until reply is received (rendezvous)
- **Receive**: Blocks receiver if mailbox is empty
- **Reply**: Unblocks the original sender

This ensures strong synchronization between tasks and prevents race conditions in message handling.
