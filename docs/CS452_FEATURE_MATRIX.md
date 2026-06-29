# CS452 ROTOS Feature Matrix
> **This repository:** GalateaOS — incomplete (layer0–2); APU not yet


**Canonical reference:** [CS452ROTOS-DarcyOS](https://github.com/codejedi-ai/CS452ROTOS-DarcyOS)

Copy of the family-wide matrix lives in every OS repo under `docs/`. Update the central copy in the meta workspace when the family changes.

## Unified I/O stack (interrupt-driven console)

All active train kernels use the **same IRQ split-UART1 model**:

```
gic_init()
  → route_interrupt(UARTINTER=153) + enable_RX_and_TX()  (CONSOLE RXIM + RTIM)
  → KernelCreate(io_notifier)
  → KernelCreate(UART2_MARKLIN_server)
  → KernelCreate(UART1_CONSOLE_server)
ExceptionASYNC: CONSOLE RXIC + RX-timeout (bit 6) → burst FIFO queue
Shell/UI: ConsolePoll / ConsoleGetc via UART1_CONSOLE_server (not raw uart_getc polling)
```

| Layout | UART stack location |
|--------|---------------------|
| **K-line** (DarcyOS, IrisOS, MekkanaOS, PrimeOS, TRAINS) | `src/k4/servers/{io_notifier,UART1_CONSOLE_server,UART2_MARKLIN_server,io_api}/` |
| **NIX** (KatarOS, NyxOS, AtariOS) | `src/layer3-services/uart/` (same logic, NIX include paths) |

Polling `uart_getc` remains only as a **fallback** when the console server is not registered (early boot / tests).

## Unified Docker build

Every kernel repo builds with the shared platform image:

| Item | Value |
|------|-------|
| Image | `codejedi-ai/cs452rotos-platform:latest` |
| Override | `DARCYOS_IMAGE=…` |
| Toolchain in container | `XDIR=/opt/toolchain` |
| Ensure script | `CS452ROTOS-PLATFORM/scripts/ensure-image.sh` |

**Standard commands** (all repos):

```bash
./dev.sh build-image    # pull platform image (or local PLATFORM build)
./dev.sh make all       # compile inside container
./dev.sh run            # build + QEMU interactive boot
./dev.sh shell          # dev shell in container
```

NIX repos (KatarOS, NyxOS) also support `./dev.sh test`, `pi`, `link-test`, and docker-compose `screen`/`prod` services.

## Multi-core / APU

| Line | Cores 1–3 | Server model | Doc |
|------|-----------|--------------|-----|
| NIX (KatarOS, NyxOS, AtariOS) | `accel_dispatch` + **APUServer** | One coordinator; per-core bare-metal workers | [APU_MULTI_CORE.md](APU_MULTI_CORE.md) |
| K-line | Not yet ported | Planned: `k4/servers/apu/` | same |

**Feasible extension:** per-core listener tasks with typed `{opcode, params}` job descriptors (see APU doc).

---

| Repo | OS | Layout | Console UART | Shell | APU | Build | Status |
|------|-----|--------|--------------|-------|-----|-------|--------|
| DarcyOS | DarcyOS | k0–k4 + tc1 | **IRQ split UART1** | `commands_shell` | planned | `./dev.sh make all` | **Canonical** |
| IrisOS | IrisOS | same as DarcyOS | **IRQ split UART1** | same | planned | `./dev.sh make all` | DarcyOS fork |
| MekkanaOS | MekkanaOS | k0–k4 + tc1 | **IRQ split UART1** | after K-tests | planned | `./dev.sh make MODE=qemu all` | Refactor line |
| KatarOS | KatarOS | layer0–5 | **IRQ split UART1** | display shell | **APUServer** | `./dev.sh make all` | Production NIX |
| NyxOS | NyxOS | layer0–5 | **IRQ split UART1** | display shell | **APUServer** | `./dev.sh make all` | Docker prod |
| AtariOS | AtariOS | layer0–5 | **IRQ split UART1** | display + `ConsoleGetc` | **APUServer** | `./dev.sh make all` | NIX + UART aligned |
| GalateaOS | GalateaOS | layer0–2 only | none | none | none | `./dev.sh make all` | Incomplete |
| PrimeOS (TRAINS) | PrimeOS | k0–k4 + tc1 | **IRQ split UART1** | `commands_shell` | planned | `./dev.sh make all` | Course hand-in |
| PrimeOS (mirror) | PrimeOS | same | **IRQ split UART1** | `commands_shell` | planned | `./dev.sh make all` | GitHub mirror |
| PLATFORM | — | Docker | — | — | — | `./build.sh` | Shared toolchain/QEMU |

## Repos intentionally not converted (yet)

- **GalateaOS** — layer0–2 only; no UART server stack until layer3 exists.

## See also

- [CODE_EVOLUTION_TREE.md](CODE_EVOLUTION_TREE.md) — how features evolved across the family (not git log)
