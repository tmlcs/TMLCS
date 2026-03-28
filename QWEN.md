# GLOBEX_OS - Project Context

## Project Overview

**GLOBEX_OS** is a freestanding 64-bit x86_64 kernel written in C++ and assembly. It is a hobby operating system kernel that boots via GRUB, implements its own memory management subsystems, interrupt handling, and basic drivers (VGA, serial, PIT).

**Key characteristics:**
- **Freestanding**: No standard library (`-ffreestanding`, `-fno-exceptions`, `-fno-rtti`, `-nostdlib`)
- **C++17**: Uses modern C++ features without exceptions or RTTI
- **Identity-mapped memory**: 2 GiB mapped (0x00000000–0x7FFFFFFF)
- **In-kernel testing**: All tests compiled into kernel binary, run on every boot

---

## Repository Layout

```
GLOBEX_OS/
├── kernels/x86_64/        # All kernel development happens here
│   ├── src/               # Kernel source code
│   │   ├── arch/x86_64/   # Boot assembly, GDT, IDT, paging
│   │   ├── core/          # stdint, log, panic, debug, coverage
│   │   ├── drivers/       # VGA, serial, PIT, console/print
│   │   ├── kernel/        # kernel_main entry point
│   │   ├── lib/           # string, spinlock, utils
│   │   └── memory/        # bitmap, early_alloc, slab, heap, guard
│   ├── tests/             # In-kernel test suite (linked into kernel.bin)
│   ├── targets/x86_64/    # linker.ld, ISO skeleton
│   ├── build/             # Build artifacts (generated)
│   └── dist/              # Output binaries (generated)
├── docs/                  # Documentation
│   ├── CICD_GUIDE.md      # CI/CD and static analysis guide
│   └── DOCUMENTATION_STYLE_GUIDE.md  # English-only documentation policy
├── scripts/               # Utility scripts
│   ├── analyze.sh         # Static analysis (clang-tidy + cppcheck)
│   ├── format.sh          # Code formatting (clang-format)
│   └── verify_tests.sh    # Test result verification
└── .github/workflows/     # GitHub Actions CI/CD
```

**Important:** All `make` commands must be run from `kernels/x86_64/`.

---

## Building and Running

### Prerequisites

| Tool | Purpose |
|------|---------|
| `g++` | C++ compiler (freestanding) |
| `nasm` | Assembler for `.asm` files |
| `ld` | Linker |
| `grub-mkrescue` | Creates bootable ISO |
| `qemu-system-x86_64` | Emulator |
| `clang-tidy` | Static analysis (optional) |
| `cppcheck` | Static analysis (optional) |
| `clang-format` | Code formatting (optional) |

Install on Ubuntu/Debian:
```bash
sudo apt-get install g++ nasm binutils grub-common grub-pc-bin xorriso qemu-system-x86 clang-tidy cppcheck clang-format
```

### Build Commands

```bash
cd kernels/x86_64

make build-x86_64          # Compile, link, and build ISO
make rebuild               # Clean and rebuild
make clean                 # Remove build artifacts
make distclean             # Remove all generated files

make LOG_LEVEL=0 rebuild   # Debug build (all logs)
make LOG_LEVEL=1 rebuild   # Info build (default)
make LOG_LEVEL=2 rebuild   # Warn build
make LOG_LEVEL=3 rebuild   # Error build
```

### Run Commands

```bash
make run                   # Run in QEMU with display
make run-debug             # Run with debug flags (-d guest_errors,unimp)
make run-serial            # Run headless, output to serial_output.log
make run-stdio             # Run with serial on stdout
```

### Static Analysis and Formatting

```bash
# From project root
./scripts/analyze.sh       # Run clang-tidy and cppcheck
./scripts/format.sh        # Format all C++ files
./scripts/verify_tests.sh serial_output.log  # Verify test output
```

---

## Toolchain Configuration

### Compiler Flags

```makefile
CFLAGS := -ffreestanding -fno-exceptions -fno-rtti \
          -fno-stack-protector -nostdlib -nostartfiles \
          -mno-sse -mno-sse2 -mno-mmx -mno-avx -mno-red-zone \
          -O2 -Wall -Wextra -Wpedantic -Werror -Wuninitialized
```

**Critical notes:**
- All warnings are errors (`-Werror`)
- No SSE/AVX instructions allowed
- No standard library headers available
- Use `src/core/stdint/` types instead

### Assembly Flags

```makefile
ASFLAGS := -f elf64 -g
```

### Linker Script

Located at `kernels/x86_64/targets/x86_64/linker.ld`:
- Kernel loads at physical address `0x100000` (1 MB)
- Kernel size limited to 10 MB
- Separate `.boot.data` section for page tables and stack

---

## Architecture Overview

### Boot Sequence

1. **GRUB** loads the multiboot2 kernel at physical `0x100000` (1 MB)

2. **`main.asm`** (32-bit):
   - Checks CPUID/long-mode support
   - Builds 2-level page tables (identity-maps 0–2 GiB with 2 MiB huge pages)
   - Enables PAE + long mode + paging + CR0.WP
   - Jumps to `long_mode_start`

3. **`main64.asm`** (64-bit):
   - Zeroes BSS (`__bss_start`…`__bss_end`)
   - Fixes 16-byte stack alignment
   - Calls `kernel_main()`

4. **`kernel_main()`** (`src/kernel/main.cpp`):
   - Initializes serial → VGA → GDT → IDT/IRQ → PIT → heap
   - Runs all tests
   - Halts

### Memory Map

| Range | Contents |
|-------|----------|
| `0x000000–0x100000` | BIOS / GRUB reserved |
| `0x100000` (1 MB) | Kernel load base (`__kernel_start`) |
| After kernel | Bitmap allocator region |
| `.boot.data` section | Page tables (4 × 4 KB) + guard page (4 KB) + kernel stack (64 KB) |

### Memory Subsystem (Layered)

```
kmalloc / kfree / krealloc / kmem_free_auto   (heap.cpp)
        |                     |
    slab allocator         bitmap allocator
    (≤ 2048 bytes,         (page granularity,
     7 fixed caches)        g_page_alloc_count[])
        \                  /
         early_alloc (bump allocator, boot-time only)
```

| Allocator | Purpose |
|-----------|---------|
| `early_alloc` | Bump pointer, used before `heap_init()` |
| `bitmap` | One bit per 4 KB page; tracks physical page allocation |
| `slab` | 7 caches: 32, 64, 128, 256, 512, 1024, 2048 bytes |
| `heap` | `kmalloc_align(size, alignment)` supports `alignment ≤ PAGE_SIZE` |
| `guard` | Stack guard page protection |

### Interrupt Handling

**GDT**: 7 entries — null, kernel code/data, user code/data, TSS (two consecutive 8-byte entries per x86_64 spec)

**IDT**: Vectors 0–31 CPU exceptions, 0x20–0x2F hardware IRQs

**ISR frame layout** (`interrupts.asm`):
```
r15…rax (15 regs, pushaq) | int_num | err_code | CPU-pushed rip, cs, rflags, rsp, ss
```

After `popaq`, `add rsp, 16` discards `int_num + err_code` before `iretq`.

### Spinlock

`spinlock_t` in `src/lib/spinlock/` is a **non-reentrant interrupt-disabling CAS spinlock**:

- **Always initialise with `= SPINLOCK_INIT`** for global/static locks
- Acquiring disables interrupts on the current CPU
- Recursive acquisition on the same CPU **deadlocks** (spins forever with interrupts disabled)

---

## Key Conventions

### No Standard Library

- Use `src/core/stdint/` types (`uint32_t`, `size_t`, etc.)
- No `<stdio.h>`, `<string.h>`, or `<stdlib.h>`
- Use kernel's own `string.h` / `string.cpp`

### Deprecated Functions

- **`strcpy` is `[[deprecated]]`** — use `strlcpy(dest, src, sizeof(dest))` instead
- Test files exercising `strcpy` must suppress with:
  ```cpp
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  ```

### Format Specifiers

The kernel's `log.cpp` printf-like formatter supports:

| Specifier | Status |
|-----------|--------|
| `%u`, `%d` | Supported |
| `%llu`, `%lld` | Supported (64-bit) |
| `%p` | Supported |
| `%zu` | Supported |
| `%s`, `%x`, `%X` | Supported |
| **`%lu`** | **NOT supported** — produces incorrect output silently |

### Logging

Compile-time log level via `LOG_MAX_LEVEL` (passed as `-DLOG_MAX_LEVEL=N`):

| N | Level |
|---|-------|
| 0 | `LOG_LEVEL_DEBUG` |
| 1 | `LOG_LEVEL_INFO` (default) |
| 2 | `LOG_LEVEL_WARN` |
| 3 | `LOG_LEVEL_ERROR` |
| 4 | `LOG_LEVEL_PANIC` |

Usage:
```cpp
LOG_DEBUG("Debug message: %d", value);
LOG_INFO("Info message: %s", str);
LOG_WARN("Warning: %p", ptr);
LOG_ERROR("Error: %x", hex);
LOG_PANIC("Fatal: %s", msg);  // Never returns
```

`LOG_DEBUG(fmt, ...)` etc. are zero-cost when compiled out.

### Type-Safe Wrappers

Functions use strong typedefs to prevent parameter-swap bugs:

```cpp
// Use wrapper constructors
idt_set_gate(handler_addr((uint64_t)isr0), ...);
gdt_set_entry(type_attr((uint8_t)attr), ...);
vga_put_char(col(0), row(0), ...);
```

### Memory Barriers

`src/arch/x86_64/include/barriers.h`:

| Macro | Effect |
|-------|--------|
| `mb()` | Compiler barrier (sufficient for x86_64 TSO) |
| `rmb()` / `wmb()` | Compiler-only read / write barriers |
| `hw_mb()` | Hardware fence: `lfence` + `sfence` |

---

## Development Workflow

### Code Formatting

All C++ code follows `.clang-format` (LLVM-based, 4-space indent, 100-char limit):

```bash
# Format all files
./scripts/format.sh

# Or manually
clang-format -i src/**/*.cpp src/**/*.h
```

### Static Analysis

```bash
# Run both clang-tidy and cppcheck
./scripts/analyze.sh

# Verbose output with logging
./scripts/analyze.sh 2>&1 | tee analyze.log
```

### CI/CD Pipeline

GitHub Actions (`.github/workflows/ci-cd.yml`) runs on every push/PR:

| Job | Description | Required |
|-----|-------------|----------|
| `build` | Compiles the kernel | ✅ |
| `test` | Runs tests in QEMU | ✅ |
| `static-analysis` | clang-tidy + cppcheck | ⚠️ Advisory |
| `code-style` | Verifies formatting | ✅ |

### Documentation Standards

**All documentation and comments must be in English** (per `docs/DOCUMENTATION_STYLE_GUIDE.md`).

Use Doxygen-style comments for public APIs:
```cpp
/**
 * @brief Clear function description
 * @param param_name Parameter description
 * @return Return value description
 * @note Optional notes
 */
```

---

## Testing

Tests live in `kernels/x86_64/tests/` and are compiled into the kernel binary. `kernel_main()` calls each `test_*()` function in sequence.

### Test Output

Output goes to serial (`serial_write`). The authoritative test run:
```bash
make run-serial
cat serial_output.log
```

Look for lines ending in `PASSED` or `FAILED`.

### Adding a Test

1. Create `tests/test_foo.{h,cpp}`
2. Add `#include` and call in `kernel_main()`
3. Add object to Makefile's `TEST_SRCS` (automatic via `find`)

### Test Files

| Test | Purpose |
|------|---------|
| `test_bss` | BSS zero-initialization |
| `test_color` | VGA color verification |
| `test_debug` | Debug macros |
| `test_gdt_idt` | GDT/IDT initialization |
| `test_print` | Console output functions |
| `test_serial` | Serial driver |
| `test_string` | String functions |
| `test_strlcpy` | Safe string copy |
| `test_slab` | Slab allocator |
| `test_spinlock` | Spinlock (init, acquire/release, SMP) |
| `test_pit` | PIT timer |
| `test_log` | Logging system |
| `test_memory_manager` | kmalloc/krealloc/kcalloc |

---

## Common Pitfalls

### BSS Not Zeroed

If `test_bss_variable != 0`, the boot code has a critical bug. The kernel will early-panic before any initialization.

### Spinlock Deadlock

```cpp
// WRONG - recursive acquisition deadlocks
spinlock_lock(&lock);
spinlock_lock(&lock);  // Spins forever!

// Also WRONG - holding lock while calling code that acquires same lock
spinlock_lock(&lock);
LOG_INFO("message");  // May deadlock if log uses same lock
```

### Format Specifier Bug

```cpp
// WRONG - %lu not supported
LOG_INFO("value: %lu", unsigned_long_value);  // Silent garbage output

// CORRECT
LOG_INFO("value: %llu", (unsigned long long)unsigned_long_value);
```

### Alignment Restrictions

```cpp
// WRONG - alignment > PAGE_SIZE returns NULL
void* p = kmalloc_align(100, 8192);  // Returns NULL

// CORRECT - alignment ≤ PAGE_SIZE
void* p = kmalloc_align(100, 4096);  // OK
```

---

## File Conventions

### EditorConfig

Follows `.editorconfig`:
- **C++/H**: 4-space indent, no tabs, LF line endings
- **Assembly**: 8-space tabs
- **Makefile**: 8-space tabs
- **Markdown**: No trailing whitespace trim

### Naming Conventions

- **Files**: `snake_case.cpp`, `snake_case.h`
- **Functions**: `snake_case()`
- **Types**: `snake_case_t` for typedefs/structs
- **Constants**: `UPPER_SNAKE_CASE`
- **Tests**: `test_<feature>.cpp`

---

## Quick Reference

### Essential Commands

```bash
cd kernels/x86_64
make rebuild run-stdio     # Build and run with serial output
make run-serial            # Run headless, check serial_output.log
make LOG_LEVEL=0 rebuild   # Debug build with all logs
```

### From Project Root

```bash
./scripts/format.sh        # Format code
./scripts/analyze.sh       # Static analysis
./scripts/verify_tests.sh serial_output.log  # Check test results
```

### Verify Toolchain

```bash
cd kernels/x86_64
make verify-tools
```

---

## References

- `CLAUDE.md` — Detailed architecture documentation
- `docs/CICD_GUIDE.md` — CI/CD and static analysis guide
- `docs/DOCUMENTATION_STYLE_GUIDE.md` — Documentation standards
- `kernels/x86_64/Makefile` — Build system
- `kernels/x86_64/targets/x86_64/linker.ld` — Linker script
