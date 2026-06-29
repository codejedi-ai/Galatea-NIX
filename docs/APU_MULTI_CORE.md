# APU / Multi-Core Dispatch (Cores 1–3)

**Canonical implementation:** **`CS452ROTOS-APU-DarcyOS`** (this repo). KatarOS, NyxOS, and AtariOS are APU-line variants with display UI.

**Status:** DarcyOS APU — terminal-only, **3 per-core servers**, completion SGI, stack-only memory (no heap).

## Hardware model (QEMU `raspi4b` / Pi 4)

| Core | Role | Scheduler? |
|------|------|------------|
| **CPU0** | Full microkernel + all kernel tasks | Yes |
| **CPU1–3** | Bare-metal accelerator workers (`secondary_worker_c`) | No — WFE loop only |

At boot, core 0 releases cores 1–3 from the QEMU spin-table mailboxes (`0xE0`–`0xF0`).

## Architecture: one server per APU

```
Client task  →  Send(APU{N}_server, {fn, arg})
APU{N}_server → accel_dispatch(N, fn, arg)
APU{N}_server → AwaitEvent(APU{N}_DONE)   /* yields CPU until SGI */
APU worker   → fn(arg); apu_signal_completion(N)  /* SGI → CPU0 */
APU{N}_server → Reply(client)
```

| Piece | DarcyOS APU |
|-------|-------------|
| Servers | `APU1_server`, `APU2_server`, `APU3_server` (one task each) |
| Job RAM | 4 KB stack workspace per server (`APU_JOB_STACK_RAM`) — no heap |
| Completion | GIC SGI 1/2/3 → `AwaitEvent` unblocks waiting server |
| UI | UART terminal only (`terminal_shell.c`) — no DisplayServer |

## Client API

```c
int APUDispatch(int core_or_any, void (*fn)(void *), void *arg);
int APUBatch(const ApuJob *jobs, int n);   /* up to 3 parallel */
```

Shell commands: `apu` (stats), `aputest` (parallel smoke test).

## Constraints

1. **No heap** — malloc/task_heap removed from build.
2. **No malloc on secondary cores** — pass stack/static buffers from core 0.
3. **Cache coherency**: `dmb ish` / `dsb sy` in `accel_dispatch`.
4. **UART / GIC**: only core 0 handles IRQ-driven I/O.

## Key files

| Path | Purpose |
|------|---------|
| `layer1-processes/accel.{c,h}` | Bare-metal job slots |
| `layer1-processes/apu_completion.{c,h}` | SGI completion to CPU0 |
| `layer3-services/apu_core_server.{c,h}` | Per-core APU servers |
| `layer3-services/apu_client.{c,h}` | Client stubs |
| `layer1-processes/terminal_shell.c` | UART-only shell |
