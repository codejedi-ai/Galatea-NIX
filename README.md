# CS452ROTOS-APU-SmpOS

**OS name:** SmpOS  
**Family:** **APU-line — canonical reference** (like DarcyOS for K-line). Full NIX stack (`layer0-assembly` … `layer5-applications`), **APUServer** + accel workers on cores 1–3, IRQ UART, display shell, TC1. **SMP multikernel** (`CoreSend`, IPI, LNS/CNS) is developed here on top of the APU model.  
**Git provider:** codejedi-ai  
**Origin repository:** [github.com/codejedi-ai/CS452ROTOS-APU-SmpOS](https://github.com/codejedi-ai/CS452ROTOS-APU-SmpOS)  

KatarOS, NyxOS, and AtariOS are **APU-line variants** that track this repo.

Formerly **CS452ROTOS-SMP-SmpOS** / GalateaOS / `Galatea-NIX` on GitHub.

## Build

```bash
./dev.sh build-image    # pull shared platform image (once)
./dev.sh make all       # kernel → os/0-d273liu.img
./dev.sh run            # interactive QEMU (OS terminal)
./dev.sh test           # timed smoke boot
./prod.sh up            # browser screen @ http://localhost:7681
```

## Layout

| Layer | Role |
|-------|------|
| layer0-assembly | Boot, vectors, context switch, secondary-core entry |
| layer1-processes | Scheduler, syscalls, GIC, accel, VM |
| layer2-messaging | Send / Receive / Reply |
| layer3-services | Nameserver, clock, IRQ UART, APUServer, display, Marklin |
| layer4-ui | Shell, canvas, display client |
| layer5-applications | Train control, games, tests |

## Docs

- [structure.md](docs/structure.md) — folder map
- [DOCS_INDEX.md](docs/DOCS_INDEX.md) — documentation index

## Family documentation

| Doc | Description |
|-----|-------------|
| [docs/CS452_FEATURE_MATRIX.md](docs/CS452_FEATURE_MATRIX.md) | Feature parity across all ROTOS kernels |
| [docs/CODE_EVOLUTION_TREE.md](docs/CODE_EVOLUTION_TREE.md) | Architecture evolution (not git history) |
| [docs/APU_MULTI_CORE.md](docs/APU_MULTI_CORE.md) | APU worker model (**canonical home: this repo**) |
| [docs/SMP_MULTIKERNEL_IPC.md](docs/SMP_MULTIKERNEL_IPC.md) | SMP target architecture |
| [docs/SMP_ROADMAP_ALL_KERNELS.md](docs/SMP_ROADMAP_ALL_KERNELS.md) | Per-kernel SMP implementation plans |

## CS452 ROTOS family

| Line | Canonical repo |
|------|----------------|
| **SMP-line** | **DarcyOS** |
| **APU-line** | **SmpOS** (this repo) |

Other APU variants: **KatarOS**, **NyxOS**, **AtariOS**.
