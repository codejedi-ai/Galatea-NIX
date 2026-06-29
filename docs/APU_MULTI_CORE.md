# APU / Multi-Core Dispatch (Cores 1–3)

**Status:** Partially implemented on NIX-line kernels (KatarOS, NyxOS, AtariOS). Planned for K-line (DarcyOS family).

## Hardware model (QEMU `raspi4b` / Pi 4)

| Core | Role today | Scheduler? |
|------|------------|------------|
| **CPU0** | Full microkernel + all kernel tasks | Yes |
| **CPU1–3** | Bare-metal accelerator workers (`secondary_worker_c`) | No — WFE loop only |

At boot, core 0 releases cores 1–3 from the QEMU spin-table mailboxes (`0xE0`–`0xF0`). Each secondary core runs a tight **WFE → execute job → SEV** loop at EL2 with interrupts disabled.

## Your idea: one server per CPU

> Each external CPU has a **server** that listens and runs a code / process / function with **parameters** on that particular chip.

**Verdict: feasible**, with two levels of ambition.

### Level A — Feasible now (recommended next step)

Keep cores 1–3 **outside** the OS scheduler, but treat each core as a **dedicated worker** with a typed command queue:

```
Client task  →  Send(APU_coordinator, {core, opcode, params[]})
Coordinator  →  accel_dispatch(core, worker_stub, &job_desc)
Core N       →  worker_stub reads job_desc, runs known handler[opcode](params)
Core N       →  SEV; coordinator Reply(client)
```

| Piece | Today | Proposed |
|-------|-------|----------|
| Coordinator | `apu_server` (one task, serial Receive) | Same, or split into router |
| Per-core listener | `accel_jobs[N].fn` + `arg` (raw function pointer) | **Job descriptor**: `{opcode, param_blob, len}` |
| Params | Single `void *arg` | Small fixed struct or shared-memory page |
| “Process” on chip | Any `void (*)(void*)` at EL2 | **Registered handlers** per core (safer than arbitrary code) |

This matches your mental model (server on each chip executes work with params) without requiring a full per-core scheduler.

**Already in tree:**

- `src/layer1-processes/accel.c` — `accel_dispatch` / `accel_wait`
- `src/layer3-services/apuserver.c` — `APU_MSG_DISPATCH`, `APU_MSG_BATCH`
- `src/layer3-services/apu_client.c` — `APUDispatch`, `APUBatch`

### Level B — Full per-core kernel servers (harder)

Run a real kernel task (`Receive` / `Reply`) pinned to each core:

- Per-core run queues, IPI for reschedule, EL0/EL1 on secondaries
- Essentially **SMP microkernel** — large CS452-scale project

Not required for train/display offload (canvas band fill, screen diff, sensor math).

## Current API (NIX line)

```c
/* Client — blocks until work completes on the chosen core */
int APUDispatch(int core_or_any, void (*fn)(void *), void *arg);
int APUBatch(const ApuJob *jobs, int n);   /* up to 3 parallel jobs */

/* Low-level — only inside apu_server or early boot tests */
void accel_dispatch(int core_id, accel_fn_t fn, void *arg);
void accel_wait(int core_id);
```

Primary uses today: parallel display canvas bands, screen-buffer diff (APU1–3 compute; CPU0 drains UART).

## Roadmap (family-wide)

| Phase | Goal | Repos |
|-------|------|-------|
| **0** | Document + matrix | All |
| **1** | Port `accel` + `apu_server` to K-line (`k3` or `k4`) | DarcyOS, PrimeOS, MekkanaOS |
| **2** | Typed job descriptors (opcode + params) instead of raw fn pointers | NIX first |
| **3** | Optional per-core named servers (`APU1_server` …) as thin wrappers over job slots | If needed for clarity |
| **4** | SMP scheduler on secondaries | Future / research |

## Constraints to remember

1. **No malloc on secondary cores** unless you pass pre-allocated buffers from core 0.
2. **Cache coherency**: `accel_dispatch` uses `dmb ish` / `dsb sy` — keep job descriptors in normal cacheable memory.
3. **UART / GIC**: only core 0 handles IRQ-driven I/O today; secondaries should not touch PL011 directly.
4. **QEMU vs Pi**: spin-table release is QEMU-specific; real Pi 4 needs PSCI or platform-specific secondary boot.

## Related files

| NIX path | Purpose |
|----------|---------|
| `layer0-assembly/cores.S` | `secondary_entry`, spin-table |
| `layer1-processes/accel.{c,h}` | Bare-metal job slots |
| `layer3-services/apuserver.c` | Coordinator server |
| `layer3-services/apu_client.{c,h}` | Client stubs |
| `layer3-services/apu_messages.h` | Wire protocol |

K-line ports would mirror under `src/k4/servers/apu/` (proposed).
