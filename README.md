# Galatea-NIX

A layered, bare-metal OS kernel for **AArch64** (e.g. Raspberry Pi), with a minimal kernel and optional train-control application layer.

## Overview

The repo uses **five layer folders**: `layer0-assembly`, `layer1-processes`, `layer2-messaging`, `layer3-services`, `layer4-application`. There are no separate `servers/`, `tc1/`, `ui/`, or `workers/` directories.

- **Layer 0 (assembly):** Boot, exception vectors, context stubs and low-level hardware interface.
- **Layer 1 (process):** Process management, scheduler, syscalls, drivers (UART, GIC, timer), utilities.
- **Layer 2 (messaging):** Inter-process communication, message passing mechanisms.
- **Layer 3 (services):** System services like nameserver, clock server, I/O server, gameserver — used by kernel and applications.
- **Layer 4 (application):** Shell, train control (track server, marklin worker, tc1 commands), tests. Train logic lives here; the kernel does not start it.

For a folder-by-folder description of the repo, see **[docs/structure.md](docs/structure.md)**.

## Requirements

- **aarch64-none-elf** toolchain (e.g. from course `xdev` or [ARM GNU toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)).
- Makefile expects the compiler in `$(XDIR)/bin`; adjust `XDIR` at the top of `Makefile` if needed.

## Build

```bash
make          # build 0-d273liu.elf and 0-d273liu.img
make clean    # remove .o, .d, .elf, .img (including under docs and base dir)
```

## Output

- **0-d273liu.elf** — Kernel ELF.
- **0-d273liu.img** — Binary image for bootloader (e.g. U-Boot).

## Documentation

- **[docs/structure.md](docs/structure.md)** — What each folder and main file does.
- **[docs/README.md](docs/README.md)** — Index of docs.
