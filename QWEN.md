# GLOBEX_OS - Project Context

## Project Overview

**GLOBEX_OS** is a 64-bit hobby operating system kernel written in C++ and x86_64 assembly. It is a freestanding kernel (no standard library) that boots via Multiboot2 and runs on x86_64 architecture.

### Key Characteristics

- **Architecture**: x86_64 (64-bit long mode)
- **Language**: C++ (freestanding, no stdlib) and NASM assembly
- **Boot**: Multiboot2-compliant (GRUB-compatible)
- **License**: BSD 3-Clause
- **Current Version**: v0.015_x64 (development branch)

### Architecture

```
0_008_x64/
├── src/
│   ├── intf/          # Public API headers (C-compatible)
│   │   ├── constants.h   # VGA, Multiboot, memory constants
│   │   ├── debug.h       # Debug macros (DEBUG_PRINT, DEBUG_ASSERT, etc.)
│   │   ├── decimal_utils.h
│   │   ├── panic.h       # Kernel panic functions
│   │   ├── print.h       # VGA text mode output API
│   │   ├── serial.h      # UART serial port API
│   │   ├── spinlock.h    # Spinlock implementation for SMP
│   │   ├── string.h      # String utilities
│   │   └── vga.h         # Low-level VGA hardware access
│   └── impl/
│       ├── kernel/      # Kernel entry point
│       │   └── main.cpp    # kernel_main() - OS initialization & tests
│       ├── tests/       # Test modules (BSS, memory, color, debug, print, etc.)
│       └── x86_64/      # x86_64-specific implementations
│           ├── boot/       # Boot assembly (Multiboot header, 64-bit entry)
│           │   ├── header.asm  # Multiboot2 header
│           │   ├── main.asm    # Protected mode boot
│           │   └── main64.asm  # Long mode entry
│           ├── hex_utils.h
│           ├── panic.cpp   # Panic implementation
│           ├── print.cpp   # VGA text mode driver
│           ├── serial.cpp  # UART 16550 driver
│           ├── spinlock.cpp
│           ├── string.cpp  # String utilities
│           └── vga.cpp     # Low-level VGA hardware
├── targets/
│   └── x86_64/
│       ├── linker.ld    # Linker script (kernel at 1MB, 2GiB identity mapped)
│       └── iso/         # ISO build directory (GRUB structure)
│           └── boot/
│               ├── grub/
│               │   └── grub.cfg
│               └── kernel.bin
├── Makefile             # Build system (g++, nasm, ld, grub-mkrescue)
└── dist/                # Build outputs (ISO, kernel.bin)
```

### Memory Layout

- **Kernel Load Address**: 1MB (0x100000)
- **Mapped Memory**: 2GiB identity-mapped (0x00000000 - 0x7FFFFFFF)
- **Page Size**: 4KB standard, 2MB large pages
- **VGA Buffer**: 0xB8000 (text mode 80x25)

## Building and Running

### Prerequisites

Required tools (verify with `make verify-tools`):
- `g++` - C++ compiler (freestanding mode)
- `nasm` - Assembly compiler
- `ld` - GNU linker
- `grub-mkrescue` - GRUB ISO creator
- `qemu-system-x86_64` - QEMU emulator
- `clang-format` - Code formatting (optional, for development)
- `clang-tidy`, `cppcheck`, `iwyu` - Static analysis (optional)

### Build Commands

| Command | Description |
|---------|-------------|
| `make -C 0_008_x64 build-x86_64` | Build kernel for x86_64 (default target) |
| `make -C 0_008_x64 run` | Build and run in QEMU |
| `make -C 0_008_x64 run-debug` | Run with debug output (guest errors, unimp) |
| `make -C 0_008_x64 run-serial` | Run with serial output to `serial_output.log` |
| `make -C 0_008_x64 run-stdio` | Run with serial on stdio (interactive debugging) |
| `make -C 0_008_x64 clean` | Remove build artifacts (`build/`, `*.bin`) |
| `make -C 0_008_x64 distclean` | Remove all generated files (including `dist/`) |
| `make -C 0_008_x64 rebuild` | Clean and rebuild |
| `make -C 0_008_x64 verify-tools` | Check if all required tools are available |
| `make -C 0_008_x64 help` | Show all available targets |

### Test Verification

After running the kernel with serial output, verify tests with:
```bash
./scripts/verify_tests.sh test_output.log
```

### Compiler Flags

The kernel uses strict freestanding compilation:
```makefile
CFLAGS := -ffreestanding -fno-exceptions -fno-rtti \
          -fno-stack-protector -nostdlib -nostartfiles \
          -O2 -Wall -Wextra -Wpedantic -Werror \
          -I src/intf
```

## Development Conventions

### Code Structure

1. **Interface/Implementation Split**:
   - `src/intf/` - Public headers with C linkage (`extern "C"`)
   - `src/impl/` - Implementation files (C++ and assembly)

2. **Header Guards**: All headers use `#ifndef` guards with `extern "C"` for C++ compatibility

3. **Naming Conventions**:
   - Constants: `UPPER_SNAKE_CASE` (e.g., `VGA_BUFFER_ADDRESS`)
   - Functions: `snake_case` with module prefix (e.g., `print_set_color`, `serial_write_str`)
   - Types: `PascalCase` with `_t` suffix (e.g., `PrintColor_t`)

4. **Code Formatting**:
   - Based on LLVM style with 4-space indentation
   - 100 character column limit
   - Configured via `.clang-format`
   - Run `./scripts/format.sh` to auto-format

### Debugging

The project uses compile-time debug macros controlled by `DEBUG_ENABLE`:

```cpp
#define DEBUG_ENABLE 1
#include "debug.h"

DEBUG_PRINTLN("Message");
DEBUG_VAR(myVar, value);      // Prints: "[DEBUG] myVar = 0xCAFEBABE"
DEBUG_ASSERT(condition);      // Breaks if false
DEBUG_LOG("Info message");
DEBUG_PRINTF("Fmt: %x %s", val, str);  // Limited format support
```

### Testing Practices

The kernel includes comprehensive self-tests in `kernel_main()`:
- BSS initialization verification
- Memory mapping tests (high memory access)
- Color validation
- Debug macro tests
- Print function tests (64-bit, signed numbers)
- Query function tests (cursor, color)
- Serial output tests (baud rates, null pointer handling)
- String function tests (memcpy overlap detection)
- Hardware detection (CPUID)

Tests are modular with separate `.h`/`.cpp` pairs in `src/impl/tests/`.

### Error Handling

- **Kernel Panic**: Use `panic()` or `panic_simple()` for fatal errors
- **Assertions**: Use `DEBUG_ASSERT()` for development-time checks
- **No Exceptions**: C++ exceptions are disabled (`-fno-exceptions`)
- **Early Panic**: Direct VGA write fallback when both serial and VGA fail

### Static Analysis

Run static analysis tools with:
```bash
./scripts/analyze.sh
```

This runs:
- `clang-tidy` - Code style and correctness
- `cppcheck` - Static analysis for C/C++
- `iwyu` - Include what you use (header cleanup)

### Git Workflow

- **Main branch**: `dev` (development)
- **Commit style**: Conventional commits with issue tracking
  - `feat(phase4): Add constants.h for magic numbers [PC005, H002]`
  - `docs(phase4): Translate public API comments to English [MNT001]`
  - `chore(phase4): Update GRUB menu entry name [GRUB001]`

## Key Components

### VGA Text Mode Driver (`print.h`/`print.cpp`)

- 80x25 text mode at 0xB8000
- 16 colors (4-bit foreground + 4-bit background)
- Functions: `print_str()`, `print_char()`, `print_hex()`, `print_dec()`, `print_hex64()`, `print_dec_signed()`, `print_dec64_signed()`
- Query functions: `print_get_cursor()`, `print_get_color()`, `print_set_cursor()`
- SMP-safe via spinlock (`print_begin_atomic()`, `print_end_atomic()`)

### Serial Driver (`serial.h`/`serial.cpp`)

- UART 16550 compatible
- Default: COM1 (0x3F8) at 115200 baud
- Functions mirror VGA driver for dual-output debugging
- Supports signed 32-bit and 64-bit decimal output
- Error reporting API: `serial_get_error_code()`, `serial_has_failed()`

### Boot Process

1. **Multiboot2 Header** (`header.asm`) - GRUB identification
2. **Boot Assembly** (`main.asm`, `main64.asm`) - Protected mode to long mode transition
3. **Linker Script** (`linker.ld`) - Memory layout with explicit segment permissions
4. **Kernel Entry** (`kernel_main()`) - C++ initialization and tests

### Memory Sections

| Section | Purpose | Permissions |
|---------|---------|-------------|
| `.boot` | Multiboot header | R+X |
| `.text` | Code | R+X |
| `.rodata` | Constants, strings | R |
| `.data` | Initialized globals | R+W |
| `.bss` | Zero-initialized globals | R+W |
| `.boot.data` | Page tables, boot stack | R+W (NOBITS) |

## Scripts

| Script | Description |
|--------|-------------|
| `scripts/format.sh` | Auto-format all C++ source files |
| `scripts/analyze.sh` | Run static analysis tools |
| `scripts/verify_tests.sh` | Verify test output from QEMU serial console |
