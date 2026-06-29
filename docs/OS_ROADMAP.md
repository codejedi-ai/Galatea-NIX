# d273liu-nix → a QNX-cousin, ultralight OS: Design & Roadmap

**Status:** design document. Describes where the kernel is today and a phased
plan to grow it into a functional, ultralight OS with a package manager that can
install and run simple apps.

**Guiding idea:** d273liu-nix is already a small **message-passing microkernel**.
Its core IPC primitive is **Send / Receive / Reply** — the same model QNX is
built on. So the target is not a rewrite; it is to push the QNX philosophy all
the way: a *tiny* privileged kernel, and everything else (drivers, filesystem,
package manager, apps) as user-space servers and programs that talk over
messages.

---

## 1. Design goals

- **Ultralight.** Microkernel stays minimal: scheduling, memory, IPC, interrupts.
  Everything optional lives outside it. Target a kernel image in the low
  hundreds of KB.
- **QNX-style.** Drivers, the filesystem, and system services are ordinary
  processes reached via `Send/Receive/Reply`. No monolithic in-kernel drivers
  beyond the minimum needed to bootstrap.
- **Runs real apps.** Programs are separate ELF binaries loaded from a
  filesystem at runtime — not compiled into the kernel image.
- **Has a package manager.** Apps are distributed as packages (think Alpine
  `apk`: a tarball + manifest) installed onto the filesystem.
- **Reproducible builds.** All compilation and testing happen in the Docker dev
  environment; the host needs only Docker. (See §7.)

### Non-goals (at least initially)
- POSIX completeness, multi-core SMP, a windowing system, a full TCP/IP stack.
- Binary compatibility with Linux/Ubuntu apps. "Like Ubuntu" here means *the
  experience* (a shell, installable packages, running programs), not the ABI.

---

## 2. Current state (what works today)

Verified by the test suite booting under QEMU (`./dev.sh test`):

| Subsystem | State | Source |
|---|---|---|
| Boot: EL2→EL1, stack, vector table | working | `layer0-assembly/boot.S` |
| Interrupts: GICv2, sync (SVC) + async (IRQ) paths | working | `layer1-processes/gic.c`, `layer0-assembly/vector.S` |
| Context switch (Save/Begin) | working | `layer0-assembly/asm.S` |
| Scheduler: priority min-heap (+ optional Q-learning overlay) | working | `layer1-processes/syscall.c` |
| Syscalls: Create/CreateArgs/Exit/Yield/MyTid/Kill/AwaitEvent/… | working | `layer1-processes/syscall.h` |
| Timer + clock IRQ | working | `layer1-processes/timer/` |
| **IPC: Send / Receive / Reply** | working | `layer2-messaging/messaging.c` |
| Heap: mymalloc / myfree / realloc | working | `layer1-processes/malloc/malloc.c` |
| Spinlocks | working | `layer1-processes/locks.c`, `layer0-assembly/spinlock.S` |
| Process/thread model with shared-memory regions | working | `layer1-processes/syscall.c` |
| UART PL011 console | working | `layer1-processes/rpi.c` |

**The heap is one of ten subsystems — the kernel is far more than `malloc`.**

### Key limitations to be aware of
- **No isolation:** one physical address space. A stray pointer in any task can
  corrupt any other.
- **No user mode:** everything runs privileged at EL1. (The SVC trap mechanism
  exists, which is half of what EL0 separation needs.)
- **Apps are in-image:** "tasks" are C functions linked into the kernel. There
  is no way to load an external program.
- **No storage, no filesystem, no userland, no package manager.**

---

## 3. Target architecture

```
              ┌──────────────────────────────────────────────┐
  user (EL0)  │  shell   pkg-mgr   app1   app2   ...           │   ELF programs
              │     │       │        │      │                  │   loaded from FS
              ├─────┴───────┴────────┴──────┴──────────────────┤
              │  fs-server   blk-driver   uart-driver  …        │   QNX-style
              │  (VFS)       (virtio-blk) (console)             │   user servers
              └───────────────▲──────────────▲─────────────────┘
                              Send/Receive/Reply (IPC)
              ┌───────────────┴──────────────┴─────────────────┐
  kernel(EL1) │  scheduler · IPC · interrupts · timer             │   microkernel
              └─────────────────────────────────────────────────┘
```

Everything above the line is a normal process. The kernel only does what *must*
be privileged. This is the QNX shape.

---

## 4. Gap analysis → the critical path

The single most important transition is **"kernel that runs built-in tasks" →
"OS that loads and runs programs."** That requires three things together:

1. **Virtual memory** — per-process address spaces and isolation.
2. **User mode (EL0)** — run apps unprivileged; syscalls via SVC into EL1.
3. **ELF loader** — parse an ELF, map its segments into a new address space, jump
   to its entry point at EL0.

Until these exist, a filesystem or package manager has nothing to run. So the
roadmap front-loads them.

---

## 5. Phased roadmap

Each phase ends with a demoable milestone runnable via `./dev.sh test`.

### Phase 0 — Stabilize the base *(small, do first)*
- Set `USE_QL_SCHED 0` in `layer1-processes/config.h` for a deterministic
  priority scheduler during OS bringup (keep Q-learning as an opt-in experiment).
- Clean the two existing build warnings (`setup_gpio` unused; `asm.S:17` `ldp`
  writeback) so new warnings stand out.
- **Milestone:** deterministic boot + green test suite, warning-free.

### Phase 1 — User mode + per-process address spaces
- Give each process its own address space.
- Drop new processes to EL0; route their syscalls through the existing SVC path.
- Define a stable **syscall ABI** (numbers + calling convention) for user code.
- **Milestone:** an EL0 task makes a syscall (e.g. write to console) and is
  isolated from the kernel and other tasks.

### Phase 3 — ELF loader + program spawn
- `spawn(path, argv)`: read an ELF, map PT_LOAD segments into a fresh space, set
  up a user stack, start at EL0.
- Until a filesystem exists, load ELFs from an in-image `initramfs` blob.
- **Milestone:** a separately-compiled "hello" ELF runs as a user process.

### Phase 4 — Storage + filesystem (QNX-style server)
- `virtio-blk` driver over virtio-mmio (QEMU virt provides it).
- Start with an in-RAM `ramfs`; then a read-only **FAT32** reader (so images can
  be built on the host) or a tiny custom FS.
- Expose the FS as a **user-space server**: file ops are `Send/Receive/Reply`
  messages; a thin VFS in libc turns `open/read/write` into messages.
- **Milestone:** `spawn` loads an ELF *from the filesystem*, not initramfs.

### Phase 5 — Userland
- Minimal **libc** + `crt0` (syscall stubs, `malloc` over a brk/mmap syscall,
  `printf`, string/file ops). Static-linked, musl-spirit.
- A **shell** as a user program: parse a line, `spawn` programs, wait, redirect.
- A busybox-style multicall binary for `ls/cat/echo/...` to stay small.
- **Milestone:** interactive shell over the UART runs FS-resident programs.

### Phase 6 — Package manager (`gpkg`)
- **Package format:** a compressed tarball + a manifest (name, version,
  dependencies, file list, install hooks). Keep it dead simple, `apk`-like.
- **Local repo first:** packages live on a disk partition / directory; no
  network needed. `gpkg install <pkg>` = verify, unpack files into the FS,
  record in an installed-package DB; `remove`, `list`, `info`.
- **Apps as packages:** an app package drops an ELF into `/bin` + metadata; the
  shell finds and `spawn`s it.
- **Milestone:** `gpkg install hello && hello` works from a clean image.

### Phase 7 (optional) — Networking & remote packages
- `virtio-net` driver + a minimal IP/UDP/TCP stack (or a tiny HTTP client) as a
  user server, so `gpkg` can fetch from a remote repo.
- **Milestone:** `gpkg install` pulls a package over the network.

---

## 6. Package manager design sketch (`gpkg`)

```
package:  name-version.gpkg  =  tar(xz) of:
            ./MANIFEST            # name, version, deps, sha256s, hooks
            ./files/...           # payload, laid out relative to FS root

install:  read MANIFEST → check deps in installed DB → verify checksums →
          unpack ./files/* into root → run post-install hook →
          append record to /var/gpkg/installed

remove:   look up record → delete owned files → run pre-remove hook → drop record
list:     read /var/gpkg/installed
```

- **Ultralight choices:** no scripting language for hooks beyond running a packaged
  ELF; flat text DB; content-addressed checksums for integrity.
- **Security:** verify checksums always; optional signature check is a later add.

---

## 7. Build & dev workflow (already in place)

- Docker image = **environment only** (ARM `aarch64-none-elf` toolchain + QEMU);
  source is bind-mounted at `/src`, so editing code never rebuilds the image.
- The QEMU "VM" runs **inside the container** — `./dev.sh run` boots the kernel
  and the serial console becomes your interactive terminal (keyboard included).
  This is the practical "run the OS on my Mac" answer; no VMware needed.
- `./dev.sh shell | build | run | test | clean` — portable, resolves the repo
  from the script location (works wherever the GitHub repo is cloned).
- As userland appears, add targets to build the **app/libc tree** and assemble a
  **disk/initramfs image** the kernel can boot from — all inside the same
  container so the host still needs only Docker.

### Implemented so far
- **Boot test suite** runs on every boot (toggle `BOOT_RUN_TESTS` in
  `config.h`).
- **Interactive serial shell** (`layer1-processes/shell.c`) — an in-kernel
  terminal that starts after the tests (toggle `START_SHELL`). Built-ins:
  `help, echo, mem, pages, uptime, tid, clear, about`. This is "Phase 5 lite":
  the seed of userland, reusing the existing UART + cooperative `Yield()`.
- **Heap stats** (`malloc_total_bytes/free_bytes/free_blocks`) via shell `mem`.
- **Physical frame allocator** (`pmm.c`) — 4 KB page allocator over a RAM pool;
  `pmm_alloc_page/alloc_pages/free*`, stats via shell `pages`.
- **`kmmap` / `kmunmap` / `ksbrk`** (`vmm.c`) — the page-granular allocator API
  that a future libc `mmap`/`brk` syscall maps onto. Validated by `vm_selftest()`.
- **Per-task heap** — each task gets a private 256 KB heap slice from a PMM pool;
  `task_malloc()` allocates from the current task's slice (logical isolation).

### Toward running real interpreters (CPython / Node) — reality check
The user-facing ask was a libc + `mmap`/`open`/`read`/`write` so CPython/Node can
run. That sits at the **top** of the stack; the layers under it must come first:
1. **Per-process address spaces** — *next step*. Real isolation = each process
   gets its own address space. Today all tasks share one physical address space.
2. **Syscall ABI** — a stable numbered syscall interface for user binaries.
4. **ELF loader** — load an external program into a fresh space and run it.
5. **Filesystem + VFS** — backs `open/read/write/close`.
6. **libc port (musl/newlib)** — its syscall stubs target the ABI above; its
   `malloc`/`mmap`/`brk` sit on the `pmm`/`vmm`/`ksbrk` primitives already built.
7. **CPython** becomes plausible here. **Node/V8** additionally needs a JIT
   (W^X executable-page management) and a large POSIX surface — realistically a
   research-grade effort and a much later milestone, if ever.

### How far is internet access?
Networking is a **separate track** from the libc/filesystem stack — a kernel-level
network stack can reach the internet without userland, a filesystem, or libc. What
it needs, in order:
1. **virtio-mmio transport** — discover/handshake virtio devices on QEMU virt
   (virtqueue setup). Foundation for both net and block.
2. **virtio-net driver** — TX/RX over virtqueues. (Moderate.)
3. **TCP/IP stack** — port **lwIP** (designed for embedded; needs only a heap +
   timer + driver shim, all of which exist) for ARP/IP/ICMP/UDP/TCP, or write a
   minimal stack. (The biggest single piece.)
4. **DHCP + DNS** — address config and name resolution (lwIP includes both).
5. QEMU side: user-mode networking (SLIRP, `-netdev user`) needs no host setup,
   so `ping`/HTTP GET is testable immediately once the stack is up.

Rough distance: **3 substantial subsystems** (virtio-mmio, virtio-net, lwIP port).
It does not depend on the FS/libc/CPython track, so it can be pursued in parallel.
A kernel-internal "fetch a URL" demo is a realistic milestone after those three;
full sockets for *user programs* additionally needs the syscall ABI + libc.

---

## 8. Risks & realism

- This is a **multi-month** effort; Phases 1–3 are the hardest and unlock
  everything else.
- EL0 transitions are fiddly — expect debugging via QEMU + `gdb` and lots of
  register-level care.
- Keep the microkernel small: every time something *can* be a user server,
  make it one. That discipline is what keeps it a true QNX cousin and ultralight.

---

## 9. Suggested immediate next steps

1. Land **Phase 0** (deterministic scheduler + warning-free build) — low risk,
   sets a clean baseline.
2. Begin **Phase 1** (per-process address spaces + EL0) behind a build flag
   so the current working kernel stays bootable.
3. Revisit this roadmap after Phase 3 — once programs load, the filesystem and
   package-manager design may want adjusting based on what bring-up taught us.
