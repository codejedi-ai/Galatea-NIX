# CS452ROTOS-GalateaOS

**OS name:** GalateaOS  
**Git provider:** codejedi-ai  
**Origin repository:** [github.com/codejedi-ai/CS452ROTOS-GalateaOS](https://github.com/codejedi-ai/CS452ROTOS-GalateaOS)  
**This tree:** GalateaOS line — Q-learning scheduler, five-layer layout (`layer0-assembly` … `layer4-application`)

Formerly `Galatea-NIX` on GitHub.

## Build

```bash
make
make clean
```

Outputs: `0-d273liu.elf`, `0-d273liu.img`.

## Layout

| Layer | Role |
|-------|------|
| layer0-assembly | Boot, vectors, context switch |
| layer1-processes | Scheduler, syscalls, drivers |
| layer2-messaging | Send / Receive / Reply |
| layer3-services | Nameserver, clock, I/O, gameserver |
| layer4-application | Shell, train control, tests |

## Docs

- [structure.md](docs/structure.md) — folder map
- [DOCS_INDEX.md](docs/DOCS_INDEX.md) — documentation index


## Family documentation

| Doc | Description |
|-----|-------------|
| [docs/CS452_FEATURE_MATRIX.md](docs/CS452_FEATURE_MATRIX.md) | Feature parity across all ROTOS kernels |
| [docs/CODE_EVOLUTION_TREE.md](docs/CODE_EVOLUTION_TREE.md) | Architecture evolution (not git history) |
| [docs/APU_MULTI_CORE.md](docs/APU_MULTI_CORE.md) | Multi-core / APU server design and feasibility |

**Multi-core (APU):** No APU stack until layer3 services land; see [docs/APU_MULTI_CORE.md](docs/APU_MULTI_CORE.md) for the family design.
## CS452 ROTOS family

GalateaOS shares the CS452 ROTOS naming family with **DarcyOS**, **KatarOS**, **MekkanaOS**, **NyxOS** (formerly MyNixOS), **IrisOS** (CS452ROTOS-IrisOS), and **PrimeOS**. Only DarcyOS uses the DarcyOS product name.
