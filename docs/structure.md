# Project structure

Galatea-NIX is a layered OS/kernel for **AArch64** (Raspberry Pi), built with a bare-metal toolchain. The codebase uses **exactly five layers**; there are no separate `servers/`, `tc1/`, `ui/`, or `workers/` folders — those have been removed. Layer 3 contains system services; layer 4 contains applications and train logic (including shell and tc1 command parsing).

---

## Canonical folder layout

```
Galatea-NIX/
├── Makefile
├── linker.ld
├── README.md
├── docs/                    # Documentation (this file, etc.)
├── calibrationdata/        # Train calibration data (optional)
├── layer0-assembly/         # Boot and exception vectors
├── layer1-processes/        # Process management: processes, syscalls, drivers
├── layer2-messaging/        # Inter-process communication: message passing mechanisms
├── layer3-services/         # System services (nameserver, clock, I/O, gameserver)
└── layer4-application/      # Applications: shell, train control, track, tests
    └── tests/               # Per-layer test framework and tests
```

**Removed (no longer used):** `layer2-services` (moved to layer3-services), `layer2-communication`, `tc1`, `ui`, `workers`, `layer3-application/startup-test`. Build artifacts that lived there are gone; source lives in the five layers above.

---

## Root

| Item | Purpose |
|------|--------|
| **Makefile** | Builds the kernel: compiles layer0–layer3, links with `linker.ld`, produces `0-d273liu.elf` and `0-d273liu.img`. Targets: `all` (default), `clean`. |
| **linker.ld** | Linker script. Sets entry to `_start` and places `.text.boot` at `0x80000`. |
| **README.md** | Project overview and quick start. |

---

## layer0-assembly

**Role:** Lowest-level code: boot and exception handling in assembly.

| File | Purpose |
|------|--------|
| **boot.S** | First code run: drops from EL2 to EL1 (if needed), disables MMU, sets up stack, jumps to C `kmain`. |
| **asm.S** | Context switch and trap support: `Begin` (restore user context and ERET), register save/restore, `Save` / `ASYNCSave` for trap frames. |
| **vector.S** | EL1 exception vector table: sync, IRQ, FIQ, SError handlers; calls C `Handle` / `HandleASYNC` for syscalls and async (e.g. timer, UART) interrupts. |

Headers (e.g. `asm.h`) live in **layer1-processes** and declare the assembly entry points.

---

## layer1-processes

**Role:** Process management core: process/syscall handling, drivers, utilities. No application logic.

| File | Purpose |
|------|--------|
| **main.c** | `kmain`: system init, UART console only, creates nameserver, idle, clock server/notifier, and `first_user_task`; then `Schedule()`. |
| **processes.c / .h** | `first_user_task`: minimal first user task (registers, prints "Kernel ready", yields). |
| **syscall.c / .h** | Syscall dispatch (e.g. Create, Send, Receive, Reply, Yield, Exit), scheduler (priority heap), `Handle`/`HandleASYNC`, interrupt handling (timer, UART). |
| **rpi.c / .h** | Raspberry Pi HW: GPIO, UART (console + optional second line), system timer registers. |
| **gic.c / .h** | Generic Interrupt Controller: route, enable, ack, “active” interrupt state. |
| **systimer.c / .h** | Kernel timer: `get_timerLO/HI`, `set_timerC3`, `resetCS` for clock server. |
| **malloc.c** | Kernel heap allocator (used for process stacks, etc.). |
| **util.c / .h** | Helpers: `memset`, `memcpy`, `i2a`, `ui2a`, print helpers. |
| **custstr.c / .h** | Custom string: `strcmp_ret`, `parse_char_arr`, `cust_strcpy`, `strcat_cust`, `is_empty`. |
| **custmath.c / .h** | Custom math (e.g. `min`) and 64-bit helpers. |
| **int64voodoo.c / .h** | 64-bit arithmetic / helpers. |
| **debugprint.c** | Debug output helpers. |
| **asm.h** | Declarations for assembly routines in layer0 (e.g. `Begin`, `Save`, `ASYNCSave`). |

The kernel does **not** start train services, shell, or I/O servers for the second UART; those belong in the application layer.

---

## layer2-messaging

**Role:** Inter-process communication mechanisms. **This is the messaging layer** — handles message passing between processes.

| File | Purpose |
|------|--------|
| **message.c / .h** | Message passing infrastructure: `Send`, `Receive`, `Reply`, message queues. |
| **ipc.c / .h** | Inter-process communication primitives and synchronization. |

The messaging layer provides the foundation for communication between processes in higher layers.

---

## layer3-application

**Role:** Applications and train control: shell, track/train logic, **tc1 command parsing**, and tests. **All of this lives here** — there is no separate `tc1/` or `ui/` folder.

| Area | Files | Purpose |
|------|--------|--------|
| **Shell** | `shell.c`, `shell.h` | Command shell: prompts, parses commands, calls `tc1ExecuteCommands` (train commands). |
| **Train control** | `train_control.c/.h`, `train_velocity.c/.h`, `train.h` | Train speed, reverse, sensor/delay stop, `stop_at`. |
| **Track** | `track_server.c/.h`, `track_data_new.c/.h`, `track_node.h` | Track topology, sensors, switches, train state on track. |
| **Marklin I/O** | `marklin_worker.c/.h` | Talks to Marklin hardware (trains/switches/sensors) via I/O servers. |
| **TC1 commands** | `tc1tests.c/.h` | `tc1ExecuteCommands`: parses shell commands (e.g. `tr`, `sw`, `rv`, `delaystop`, `sensorstop`, `stopat`). |
| **Navigation** | `goto.c`, `goto.h` | Pathfinding, “go to” node, `stop_at` logic. |
| **Speed** | `speed_measuring.c/.h` | Speed/calibration measurement. |
| **Tests** | `tests/test_runner.c/.h`, `test_framework.c/.h`, `layer0_tests.c` … `layer3_tests.c` (+ headers) | Per-layer test entry points and test framework. |

Train services (marklin_worker, track_server, shell, tc1) are **not** started by the kernel; they are part of the application layer and would be started when running the full train application.

---

## calibrationdata

**Role:** Calibration data for train behaviour (e.g. stopping distances, speed tests). Optional; not required for the kernel to build or run.

| File | Purpose |
|------|--------|
| **Train Control_Part 1_Design process - raw data.csv** | Raw calibration data (e.g. stopping distances, speeds). |

---

## Other directories

- **.git/** — Version control (not part of build).
- **.idea/**, **.vscode/** — IDE/editor config (not part of build).

---

## Build products

- **0-d273liu.elf** — Linked kernel executable (ELF).
- **0-d273liu.img** — Binary image for loading (e.g. by U-Boot).
- **\*.o**, **\*.d** — Object and dependency files under each layer (and base); removed by `make clean`.
