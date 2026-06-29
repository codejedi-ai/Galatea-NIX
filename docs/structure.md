# Project structure

**d273liu-nix** is a layered bare-metal **AArch64** microkernel for **Raspberry Pi 4
(BCM2711)**. All kernel source lives under **`src/`**; the repo root holds build
config, Docker, and scripts. Names and artifact filenames come from
[`project.mk`](../project.mk).

---

## Canonical layout

```
d273liu-nix/
├── project.mk              # PROJECT_ID, PROJECT_NAME, kernel/docker names
├── source_project.sh       # shell helper — sources vars from project.mk
├── Makefile                # builds kernel ELF + flat .img from src/
├── README.md
├── Dockerfile              # dev image (toolchain + QEMU raspi4b)
├── Dockerfile.prod         # prod image (kernel baked in, serves browser screen @ 9090)
├── dev.sh, prod.sh         # dev/prod drivers
├── qemu-rpi4.sh, mkpi.sh, …
├── docs/                   # documentation
├── calibrationdata/        # optional train CSV (not linked into kernel)
└── src/                    # ← all kernel code
    ├── linker.ld           # link @ 0x80000 (Pi firmware load address)
    ├── library/            # shared C utilities (math, string)
    ├── layer0-assembly/    # boot, vectors, context switch (assembly)
    ├── layer1-processes/   # kernel core: scheduler, syscalls, drivers, shell, VM
    └── layer2-messaging/   # Send / Receive / Reply IPC
```

**Not in the tree yet (roadmap):** `src/layer3-services/`, `src/layer4-application/`.
See [OS_ROADMAP.md](OS_ROADMAP.md).

**Removed / archived:** UEFI boot path, QEMU `virt` platform switch, old coursework
folders. Historical notes: [docs/archive/](archive/).

---

## Root (build & run)

| File | Purpose |
|------|---------|
| **project.mk** | `PROJECT_ID`, `PROJECT_NAME`, `KERNEL_ELF`, `KERNEL_IMG`, Docker names. |
| **Makefile** | Compiles `src/` layers, links with `src/linker.ld`, emits `$(KERNEL_IMG)`. |
| **dev.sh** | Docker dev: `shell`, `build`, `test`, `run`, `pi`, `clean`. |
| **prod.sh** | Web terminal: `up`, `down`, `build`, `logs`. |

Build products at repo root (gitignored): `$(KERNEL_ELF)`, `$(KERNEL_IMG)`, `pi-boot/`.

---

## src/library/

Shared C helpers (`-Isrc/library`).

| File | Purpose |
|------|---------|
| **math.c / .h** | `min_u64`, `max_u64`, `min_int`, `max_int`. |
| **string.c / .h** | `atoi_64`, `str_to_hex`, `strcmp_ret`, `a2d`, `is_empty`, `is_hex`. |

---

## src/layer0-assembly/

Boot and exception handling.

| File | Purpose |
|------|---------|
| **boot.S** | EL2→EL1, park secondary CPUs, stack, jump to `kmain`. |
| **asm.S** | Context switch: `Begin`, save/restore, `Save` / `ASYNCSave`. |
| **vector.S** | EL1 vector table → C `Handle` / `HandleASYNC`. |
| **spinlock.S**, **syscalls.S** | Spinlock primitives, syscall stubs. |
| **tests/** | Layer 0 tests, `test_framework`, `test_runner.c`. |

Assembly API declarations (`asm.h`) live in **src/layer1-processes/**.

---

## src/layer1-processes/

Kernel core.

| Area | Files | Purpose |
|------|-------|---------|
| **Boot** | `main.c` | `kmain`: UART, init, tests, shell. |
| **Processes** | `processes.c`, `syscall.c` | Tasks, scheduler, syscalls, IRQs. |
| **Drivers** | `rpi.c`, `gic.c`, `timer/systimer.c`, `mmio_config.h` | Pi 4 UART, GIC, timer. |
| **Memory** | `malloc/`, `pmm.c`, `vmm.c` | Heap, frame pool, MMU (off by default). |
| **Shell** | `shell.c`, `shell.h` | In-kernel serial shell. |
| **Config** | `config.h`, `project.h` | Limits; `project.h` generated from `project.mk`. |
| **tests/** | `tests/layer1_tests.c` | Layer 1 test suite. |

---

## src/layer2-messaging/

IPC (QNX-style).

| File | Purpose |
|------|---------|
| **messaging.c / .h** | `Send`, `Receive`, `Reply`. |
| **tests/layer2_tests.c** | IPC integration tests. |

---

## calibrationdata/

Optional CSV for train calibration. Not part of the kernel build.

---

## docs/

| Doc | Purpose |
|-----|---------|
| **structure.md** | This file. |
| **RUN_ON_RPI.md** | Real hardware bring-up. |
| **OS_ROADMAP.md** | Future layers and apps. |
| **archive/** | Historical debugging write-ups. |
