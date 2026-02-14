# Layer 1: IPC (QNX-style) and Spinlock

## QNX-style: Message Passing

Inter-process/thread communication is primarily via **message passing** (Send, Receive, Reply). Synchronization and data exchange are done through messages.

## Spinlock (Layer 1)

A **spinlock** is provided for short critical sections over shared memory (e.g. process shared memory). It is the lowest-level primitive (no kernel/scheduler dependency; ARM64 LDXR/STXR in layer0).

- **SpinLock_Acquire(lock)** – spin until the lock is acquired.
- **SpinLock_Release(lock)** – release the lock.

Implementation: `layer0-assembly/spinlock.S` (acquire/release); `layer1-processes/locks.h` / `locks.c` (API). Test: `test_spinlock()` in `layer1-processes/tests/layer1_tests.c` (two threads increment a shared counter under the lock; final count must equal 2 × iterations).

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
| **Heap**       | test_malloc             | mymalloc/myfree (heap allocation) |
| Process        | test_processes          | MyProcessId, GetProcessSharedMem, shared memory |
| Spinlock       | test_spinlock           | Two threads increment shared counter under lock |
| Syscalls       | test_syscalls           | Syscall interface |

IPC (Send/Receive/Reply) is **Layer 2 Test #2**; see `layer2-messaging/tests/layer2_tests.c`.

All run from **run_layer1_tests()** in `layer1-processes/tests/layer1_tests.c`.
