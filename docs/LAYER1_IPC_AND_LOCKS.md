# Layer 1: IPC (QNX-style)

## QNX-style: Message Passing Only

This kernel follows the **QNX way**: inter-process/thread communication is **only** via **message passing** (Send, Receive, Reply). There are **no** spinlocks, semaphores, or other shared-memory locks. Synchronization and data exchange are done through messages.

## IPC = Inter-Process Communication

- **Send(tid, msg, msglen, reply, replylen)** – send a message to thread `tid` and block until reply.
- **Receive(tid, msg, msglen)** – block until a message arrives; store sender in `tid`, message in `msg`.
- **Reply(tid, reply, replylen)** – send reply to the thread that sent the message (unblocks the sender).

Layer 1 tests include an **IPC test** that runs Send/Receive/Reply between two threads.

## Tests (Every Feature)

| Feature        | Test                    | What it checks |
|----------------|-------------------------|----------------|
| Timer          | test_timer              | GetKernelRuntime, time advancing |
| Context switch | test_context_switch     | Create, Yield, Exit, round-robin |
| Process create | test_process_creation   | Create() |
| Process        | test_processes          | MyProcessId, GetProcessSharedMem, shared memory |
| IPC            | test_ipc                | Send/Receive/Reply (QNX-style) |
| Syscalls       | test_syscalls           | Syscall interface |

All run from **run_layer1_tests()** in `layer1-processes/tests/layer1_tests.c`.
