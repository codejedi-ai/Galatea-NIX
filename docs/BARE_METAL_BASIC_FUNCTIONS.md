# Basic Functions in the Bare-Metal Kernel

This document lists the **basic functions** that exist (or are stubbed) in the Galatea-NIX bare-metal kernel. The kernel is built with **`-nostdlib`** and **`-fno-builtin-memcpy`**, so it does **not** link the C library; it provides or stubs what it needs.

---

## 1. Dynamic memory (kernel override)

| Function | Location | Status |
|----------|----------|--------|
| `malloc(size_in_words)` | `layer1-processes/malloc/malloc.c` | ✅ Implemented (alloc for process 0) |
| `malloc_for_process(process_id, size_in_words)` | same | ✅ Implemented |
| `free(ptr)` | same | ✅ Implemented |
| `malloc_init_default()` | same | ✅ Called from `InitSys()` |
| `malloc_vram_to_phys(process_id, vram_payload)` | same | ✅ Implemented |
| `_sbrk` (newlib) | `layer1-processes/newlib_stubs.c` | ⚠️ Stub (returns -1, ENOMEM); kernel uses `malloc`/`free` instead |

---

## 2. Memory (no SIMD)

| Function | Location | Status |
|----------|----------|--------|
| `memset(s, c, n)` | `layer1-processes/util.c` | ✅ Implemented (byte loop; avoids compiler builtin/SIMD) |
| `memcpy(dest, src, n)` | same | ✅ Implemented (byte loop; `-fno-builtin-memcpy` in Makefile) |

`memmove` is **not** implemented; use `memcpy` only when regions do not overlap.

---

## 3. String / conversion

| Function | Location | Status |
|----------|----------|--------|
| `atoi_64(str)` | `library/string.c` | ✅ String to int64 (decimal, optional minus) |
| `str_to_hex(str)` | same | ✅ Hex string (with 0x) to int64 |
| `strcmp_ret(s1, s2)` | same | ✅ Returns non-zero if equal |
| `a2d(ch)` | `library/string.h` (inline), `util.c` | ✅ ASCII digit to value |
| `is_empty(str)`, `is_hex(str)` | `library/string.h` (inline) | ✅ Helpers |

**Not implemented:** `strlen`, `strcpy`, `strncpy`. None are used in the current codebase; add to `library/string.c` if needed.

---

## 4. Math / min-max

| Function | Location | Status |
|----------|----------|--------|
| `min_u64`, `max_u64` | `library/math.c` | ✅ |
| `min_int`, `max_int` | same | ✅ |

---

## 5. Number to string (util)

| Function | Location | Status |
|----------|----------|--------|
| `ui2a(num, base, buf)` | `layer1-processes/util.c` | ✅ Unsigned to ASCII |
| `i2a(num, buf)` | same | ✅ Signed to ASCII (decimal) |

---

## 6. Newlib / libc stubs (bare-metal)

These are required or referenced by newlib but not fully implemented; they are stubbed so the build and minimal runtime work.

| Symbol | Location | Status |
|--------|----------|--------|
| `_close` | `newlib_stubs.c` | Stub (returns -1) |
| `_fstat` | same | Stub (returns S_IFCHR) |
| `_getpid` | same | Stub (returns 1) |
| `_isatty` | same | Stub (returns 1) |
| `_kill` | same | Stub (returns -1) |
| `_lseek` | same | Stub (returns 0) |
| `_read` | same | Stub (returns 0) |
| `_sbrk` | same | Stub (returns (void*)-1, ENOMEM); heap is `malloc`/`free` |
| `_write` | same | Stub (returns len); I/O uses UART, not write |
| `_exit` | same | Stub (infinite loop) |
| `printf` | same | Stub (returns 0); use `uart_printf` instead |
| `errno` | same | Defined (0) |

---

## 7. Summary

- **Dynamic memory:** Implemented in kernel (`malloc`, `free`, `malloc_for_process`, init, VRAM→phys). No libc heap; `_sbrk` is a stub.
- **memcpy / memset:** Implemented in `util.c` (byte loops, no SIMD).
- **String/conversion:** `atoi_64`, `str_to_hex`, `strcmp_ret`, `a2d`, `is_empty`, `is_hex` exist; `strlen`/`strcpy`/`strncpy` are not present but unused.
- **Math:** `min_*` / `max_*` in `library/math.c`.
- **Number→string:** `ui2a`, `i2a` in `util.c`.
- **Libc-style I/O and process:** Stubbed in `newlib_stubs.c`; real I/O and process control use kernel APIs (e.g. UART, syscalls).

So the **basic functions** needed by the current bare-metal kernel **do exist** (or are stubbed where required for link). Add `strlen`/`strcpy`/`strncpy` or `memmove` in `library/` or `util.c` only if new code needs them.
