# SMP Multikernel — Target Architecture (All Kernels)

**Applies to:** every CS452 ROTOS kernel repo. Per-repo rollout: [SMP_ROADMAP_ALL_KERNELS.md](SMP_ROADMAP_ALL_KERNELS.md).

## Model

| Item | Rule |
|------|------|
| Cores | 4 × **identical microkernel** instances (multikernel), not one global SMP scheduler (initially) |
| Local IPC | `Send` / `Receive` / `Reply` — **unchanged**, same-core only |
| Cross-core | **`CoreSend(dest_core, dest_tid, buf, len)`** — explicit, not “smart Send” |
| Naming | **LNS** (local) → **CNS** (core bridge) → **Core Name Notifier** (IPI + shared memory) |
| Sync | Shared mailbox + **spinlock** + **`dmb`/`dsb`** + **IPI** (mandatory) |

**Not APU:** `accel_dispatch` runs function pointers on worker cores without a full scheduler. SMP gives each core its own kernel, ready queues, and name servers.

## Server hierarchy

```text
Core N:
  Kernel N
    ├── LNS_N     WhoIs() for tasks on core N
    ├── CNS_N     global lookup bridge
    └── Core Name Notifier_N  ↔  peers via mailbox + IPI
```

### Global WhoIs flow

1. Task on core 0: `WhoIs("remote_task")` → LNS0 miss  
2. CNS0 → notifier0 → **IPI** → notifier1 → CNS1 → LNS1 hit  
3. Cache `(name → core_id, tid)` in LNS0  
4. App uses `CoreSend(1, tid, …)` for payload

## CoreSend flow

1. Lock destination core’s inbound mailbox spinlock  
2. Write `{src_core, src_tid, len, payload}`  
3. Memory barrier (`dmb ish` / `dsb sy`)  
4. **Unlock** spinlock (never hold lock across IPI)  
5. Fire **IPI** to dest core (GIC SGI on Pi 4)  
6. Dest IRQ handler: dequeue, unblock receiver, schedule  

## Shared memory region

Linker-reserved coherent region (per-core inbound ring or slot array):

```c
struct CoreMailbox {
    volatile uint32_t lock;
    volatile uint32_t head, tail;
    struct CoreMessage slots[CORE_MAILBOX_DEPTH];
};
```

## Phased implementation (family-wide)

| Phase | Goal | All repos |
|-------|------|-----------|
| **P0** | Document + matrix | ● done |
| **P1** | Boot cores 1–3 into per-core `kmain` | SmpOS first |
| **P2** | Per-core LNS + affinity pinning | SmpOS → DarcyOS |
| **P3** | Mailbox + spinlock + minimal IPI | SmpOS → DarcyOS |
| **P4** | `CoreSend` + CNS global WhoIs | SmpOS → DarcyOS |
| **P5** | Port to NIX line; retire or coexist with APU | KatarOS → NyxOS → AtariOS |
| **P6** | SMP-line mirrors + PrimeOS opt-in | IrisOS, MekkanaOS, PrimeOS |

## K-line vs NIX code paths (when ported)

| Line | Proposed location |
|------|-------------------|
| SMP-line | `src/common/smp/` + `src/k4/servers/core_notifier/` |
| NIX | `src/layer3-services/smp/` (replace or wrap `accel/` long-term) |

## Constraints

- Lock ordering across cores (e.g. lower `core_id` first)  
- No `malloc` on secondary kernels without shared pools  
- Only core 0 owns PL011 console IRQ until partitioned  
- QEMU `raspi4b` spin-table vs real Pi PSCI — abstract in `layer0` boot  

## Related docs

- [SMP_ROADMAP_ALL_KERNELS.md](SMP_ROADMAP_ALL_KERNELS.md) — per-kernel plans  
- [APU_MULTI_CORE.md](APU_MULTI_CORE.md) — current worker model (NIX)  
- [CS452_FEATURE_MATRIX.md](CS452_FEATURE_MATRIX.md) — status columns  
