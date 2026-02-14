# Process and Thread Model

**Processes are nothing more than another way for memory management.** A process owns a shared memory region. **Threads within a process share the same memory space**—they all use that process’s region (via `GetProcessSharedMem()`). No extra semantics; just memory management.

## Concepts

| Concept | Meaning |
|--------|---------|
| **Process** | A **memory-management** unit: a process id (0..NPROCESSES-1) and a **shared memory region** (base + size). All threads in this process share this same memory space. |
| **Thread** | A runnable unit: one stack, one PC, one set of registers. Has **process_id** (which process it belongs to). Threads in the same process share that process’s memory space. Has **thread id** (TID) = `pid`, used for scheduling and `MyTid()`. |

So: **processes = memory management** (ownership of a shared region); **threads** are runnable units that are tied to a process for shared memory.

## Data structures

- **`struct process_container`** (syscall.h): `pid`, `shared_mem_base`, `shared_mem_size`. One per process; array `PROCESS_CONTAINERS[NPROCESSES]`.
- **`struct process`** (syscall.h): the **thread** descriptor. Fields include `pid` (TID), **`process_id`** (which process this thread belongs to), stack, PC, registers, priority, IPC, etc. Array `PROCS[NUMPROCS]`.

Shared memory is allocated statically: `PROCESS_SHARED_MEM[NPROCESSES][SHARED_MEM_PER_PROCESS]` (default 8 processes × 4096 bytes).

## Creation and identity

- **KernelCreate(priority, function, parent)**: Creates a **thread**. Its `process_id` is the same as the parent thread’s `process_id` (or 0 if parent is 0). So all threads created by `Create()` from one initial thread end up in the same process and share that process’s memory.
- **MyTid()**: Returns this thread’s id (TID).
- **MyProcessId()**: Returns this thread’s process id (container id). Syscall 13.
- **GetProcessSharedMem()**: Returns the base pointer of this thread’s process shared memory region. Syscall 14. Threads in the same process get the same pointer and can use it to share data.

## Constants

- **NPROCESSES**: Max number of process containers (default 8).
- **SHARED_MEM_PER_PROCESS**: Size in bytes of each process’s shared region (default 4096).
- **NUMPROCS**: Max number of threads (default 20).

Process 0 is the default; the first thread (parent 0) gets `process_id == 0`. All threads created from it get `process_id == 0` and share the same 4 KB region. A later extension could add “create new process” (new container + first thread) and “create thread in process X”.
