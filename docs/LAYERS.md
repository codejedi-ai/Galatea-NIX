# OS layers

This kernel is organised as a QNX/Neutrino-style layered microkernel. The model:

```
┌──────────────────────────────────────────────────────────┐
│ 5. USER APPLICATIONS    (unprivileged, EL0)              │
│    RPS, Tetris, Snake, Pong                              │
├──────────────────────────────────────────────────────────┤
│ 4. SYSTEM SERVICES      (unprivileged servers, EL0)      │
│    Name server, Clock server, RPS server, Link server    │
├──────────────────────────────────────────────────────────┤
│ 3. RESOURCE MANAGERS    (drivers)                        │
│    Serial/UART, AUX mini-UART link, GIC, generic timer   │
├──────────────────────────────────────────────────────────┤
│ 2. MICROKERNEL          (privileged, EL1)                │
│    IPC (Send/Receive/Reply), scheduler, syscalls         │
├──────────────────────────────────────────────────────────┤
│ 1. HARDWARE / BOARD     (bare metal)                     │
│    boot, exception vectors, context switch (BCM2711)     │
└──────────────────────────────────────────────────────────┘
```

## Where each layer lives in `src/`

| Layer | What | Source |
|---|---|---|
| 1 — Hardware/Board | reset, EL2→EL1, exception vectors, context switch, spinlocks | `layer0-assembly/` (`boot.S`, `vector.S`, `asm.S`, `cores.S`) |
| 2 — Microkernel | scheduler, syscalls, IPC | `layer1-processes/` (`syscall.c`, `main.c`), `layer2-messaging/` (`messaging.c`) |
| 3 — Resource managers | UART/serial, AUX mini-UART link, GIC, generic timer, phys-mem/heap | `layer1-processes/` (`rpi.c`, `auxuart.c`, `gic.c`, `timer/`, `pmm.c`, `vmm.c`, `malloc/`) |
| 4 — System services | name/clock/RPS servers, link server (container bridge) | `layer3-services/` (`nameserver.c`, `clockserver.c`, `rps.c`, `linkserver.c`) |
| 5 — User applications | the games | `layer5-applications/` (`snake.c`, `tetris.c`, `pong.c`, `rps_app.c`) |

The **link** (Layer 3/4) is how the OS reaches the outside world: the AUX
mini-UART driver (`auxuart.c`) + the link server/notifier (`linkserver.c`) talk
over QEMU `serial1` to `tools/vhw.py`, container-side virtual hardware (a Märklin
sim + internet gateway). See the README's "Talking to the outside world" section.

> The historical directory names (`layer0-assembly` … `layer3-services`) predate
> this 5-layer model; the table above is the authoritative mapping. Layer 5 is the
> newest directory, added for the application tier. Renaming the older dirs to
> match 1:1 (`layer1-hardware`, `layer2-microkernel`, …) is a mechanical follow-up
> — it touches every `#include` path and the Makefile, so it should be done with a
> full build verification rather than blind sed.

## Application flow

Boot runs the test suite (Levels 1–4), then launches the **Palm UI** (Layer 4):
a Palm OS-style home screen listing the Layer 5 apps. Selecting one runs it; it
returns to the launcher on quit. The Terminal is itself app (1); inside it,
`home` returns to the launcher.
