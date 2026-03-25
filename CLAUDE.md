# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository layout

```
GLOBEX_OS/
└── kernels/x86_64/        # Only kernel target; all work happens here
    ├── src/               # Kernel source
    │   ├── arch/x86_64/   # Boot, GDT, IDT, paging (NASM + C++)
    │   ├── core/          # stdint, log, panic, debug, coverage
    │   ├── drivers/       # VGA, serial, PIT, console/print
    │   ├── lib/           # string, spinlock, utils
    │   ├── memory/        # bitmap, early_alloc, slab, heap, guard
    │   └── kernel/        # kernel_main entry point
    ├── tests/             # In-kernel test suite (linked into kernel.bin)
    └── targets/x86_64/    # linker.ld, ISO skeleton
```

All `make` commands must be run from `kernels/x86_64/`.

## Build and run commands

```bash
cd kernels/x86_64

make build-x86_64          # compile + link + build ISO
make rebuild               # clean then build
make run-serial            # build, run in QEMU headless, dump serial_output.log
make run-stdio             # build, run in QEMU with serial on stdout
make run                   # build, run in QEMU with display
make run-debug             # run with -d guest_errors,unimp

make LOG_LEVEL=0 rebuild   # debug build (all log output; default is 1=INFO)
```

There is no way to run a single test. All tests run on every boot in `kernel_main()`. Test results appear in the serial output; look for lines ending in `PASSED` or `FAILED`.

## Toolchain

| Tool | Role |
|------|------|
| `g++` | C++ compiler (freestanding, `-ffreestanding -fno-exceptions -fno-rtti -nostdlib`) |
| `nasm -f elf64` | Assembler for `.asm` files |
| `ld` | Linker, script at `targets/x86_64/linker.ld` |
| `grub-mkrescue` | Builds bootable ISO |
| `qemu-system-x86_64` | Emulator |

**All warnings are errors** (`-Wall -Wextra -Wpedantic -Werror`). No stdlib headers are available; everything is in `src/core/stdint/`.

## Architecture overview

### Boot sequence

1. **GRUB** loads the multiboot2 kernel at physical 0x100000 (1 MB).
2. **`main.asm`** (32-bit): checks CPUID/long-mode, builds 2-level page tables (identity-maps 0–2 GiB with 2 MiB huge pages), enables PAE + long mode + paging + CR0.WP, jumps to `long_mode_start`.
3. **`main64.asm`** (64-bit): zeroes BSS (`__bss_start`…`__bss_end`), fixes 16-byte stack alignment, calls `kernel_main()`.
4. **`kernel_main()`** (`src/kernel/main.cpp`): initialises serial → VGA → GDT → IDT/IRQ → PIT → heap, then runs all tests and halts.

### Memory map

| Range | Contents |
|-------|----------|
| 0x000000–0x100000 | BIOS / GRUB reserved |
| 0x100000 (1 MB) | Kernel load base (`__kernel_start`) |
| After kernel | Bitmap allocator region |
| `.boot.data` section | Page tables (4 × 4 KB) + guard page (4 KB) + kernel stack (64 KB) |

The linker asserts the kernel stays under 10 MB. `__kernel_end` is exported as a linker symbol and used by `bitmap_init()` to reserve kernel pages.

### Memory subsystem (layered)

```
kmalloc / kfree / krealloc / kmem_free_auto   (heap.cpp)
        |                     |
    slab allocator         bitmap allocator
    (≤ 2048 bytes,         (page granularity,
     7 fixed caches)        g_page_alloc_count[])
        \                  /
         early_alloc (bump allocator, boot-time only)
```

- **`early_alloc`** — bump pointer, used before `heap_init()` (slab pool comes from here).
- **`bitmap`** — one bit per 4 KB page; `g_page_alloc_count[start_page]` stores page-run length to avoid forward scan.
- **`slab`** — 7 caches: 32, 64, 128, 256, 512, 1024, 2048 bytes. Slab pages are detected by `is_slab_address()` checking `slab_t.magic` at the 4 KB-aligned page start.
- **`heap`** — `kmalloc_align(size, alignment)` supports `alignment ≤ PAGE_SIZE` only; returns `NULL` for larger alignments.
- **`guard`** — splits the boot guard page from a 2 MiB huge page into 4 KB entries and marks it not-present; protects against kernel stack overflow.

### Interrupt handling

- **GDT**: 7 entries — null, kernel code/data, user code/data, TSS (two consecutive 8-byte entries per x86_64 spec).
- **IDT**: vectors 0–31 CPU exceptions, 0x20–0x2F hardware IRQs. `idt_set_gate()` takes an explicit `ist` parameter; IST1 = #DF, IST2 = NMI, IST3 = #MC.
- **`interrupts.asm`** ISR frame layout (bottom = RSP on entry to C handler):
  `r15…rax` (15 regs, `pushaq`) | `int_num` | `err_code` | CPU-pushed `rip, cs, rflags, rsp, ss`
  After `popaq`, `add rsp, 16` discards `int_num + err_code` before `iretq`.
- **IRQ macro** uses `mov edi, %1` (32-bit move → zero-extends into RDI) to pass the IRQ number per System V AMD64 ABI.

### Spinlock

`spinlock_t` in `src/lib/spinlock/` is a non-reentrant interrupt-disabling CAS spinlock.

- **Always initialise with `= SPINLOCK_INIT`** for global/static locks. Never rely on BSS zero-fill.
- Acquiring the lock disables interrupts on the current CPU before entering the CAS loop. Recursive acquisition on the same CPU spins forever with interrupts disabled (deadlock).
- `vga_get_cursor_col/row()` are intentionally unlocked — a single aligned `volatile size_t` read is hardware-atomic on x86_64; adding the lock would risk deadlock for callers inside `vga_begin_atomic()`.

### Memory barriers

`src/arch/x86_64/include/barriers.h` defines:

| Macro | Effect |
|-------|--------|
| `mb()` | Compiler barrier (prevents reordering; sufficient for x86_64 TSO) |
| `rmb()` / `wmb()` | Compiler-only read / write barriers |
| `hw_mb()` | Hardware fence: `lfence` + `sfence` (use only when true hardware ordering is needed) |

### Logging

Compile-time log level via `LOG_MAX_LEVEL` (passed as `-DLOG_MAX_LEVEL=N`):

| N | Level |
|---|-------|
| 0 | `LOG_LEVEL_DEBUG` |
| 1 | `LOG_LEVEL_INFO` (default) |
| 2 | `LOG_LEVEL_WARN` |
| 3 | `LOG_LEVEL_ERROR` |
| 4 | `LOG_LEVEL_PANIC` |

`LOG_DEBUG(fmt, ...)` etc. are zero-cost when compiled out. The log module (`src/core/log/`) owns `g_log_lock`; do not call any `LOG_*` macro while holding that lock.

### Tests

Tests live in `tests/` and are compiled into the kernel binary. `kernel_main()` calls each `test_*()` function in sequence. Output goes to serial (`serial_write_str`). The authoritative test run is `make run-serial`, which writes `serial_output.log`.

To add a test: create `tests/test_foo.{h,cpp}`, add `#include` and a call in `kernel_main()`, add the object to the Makefile's `TEST_SRCS` glob (automatic via `find`).

## Key conventions

- **No stdlib** — use `src/core/stdint/` types (`uint32_t`, `size_t`, etc.). No `<stdio.h>`, `<string.h>`, or `<stdlib.h>`. Use the kernel's own `string.h` / `string.cpp`.
- **`strcpy` is `[[deprecated]]`** — use `strlcpy(dest, src, sizeof(dest))` instead. Test files that exercise `strcpy` must suppress with `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`.
- **Format specifiers** — the kernel's `log.cpp` printf-like formatter does not support `%p` or `%zu`. Use `%s` + manual hex conversion or `%u` / `%lu`.
- **Type-safe wrappers** — `idt_set_gate`, `gdt_set_entry`, and VGA functions use strong typedefs (`handler_addr_t`, `type_attr_t`, `vga_col_t`, etc.) to prevent parameter-swap bugs. Always use the wrapper constructors (e.g., `handler_addr((uint64_t)isr0)`).
- **`slab_t` detection** — to distinguish slab from page allocations, align the pointer down to `SLAB_SIZE` (4 KB) and check `slab_t.magic == SLAB_MAGIC`. Read `slab_t.object_size` for the actual allocation size, not the page size.
- **No SSE/AVX** — compiler flags disable vector registers. Do not write code that requires them.
