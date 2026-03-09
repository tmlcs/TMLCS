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
│   │   ├── spinlock.h    # Spinlock primitives for SMP
│   │   ├── string.h      # String utilities
│   │   └── vga.h         # Low-level VGA hardware access
│   └── impl/
│       ├── kernel/      # Kernel entry point
│       │   └── main.cpp    # kernel_main() - OS initialization & tests
│       ├── tests/       # Test modules
│       │   ├── test_bss.cpp/h
│       │   ├── test_color.cpp/h
│       │   ├── test_debug.cpp/h
│       │   ├── test_hardware.cpp/h
│       │   ├── test_memory.cpp/h
│       │   ├── test_print.cpp/h
│       │   ├── test_query.cpp/h
│       │   ├── test_serial.cpp/h
│       │   ├── test_serial_signed.cpp/h
│       │   ├── test_string.cpp/h
│       │   └── test_framework.h
│       └── x86_64/      # x86_64-specific implementations
│           ├── boot/       # Boot assembly (Multiboot header, 64-bit entry)
│           │   ├── header.asm  # Multiboot2 header
│           │   ├── main.asm    # 32-bit protected mode entry
│           │   └── main64.asm  # 64-bit long mode entry
│           ├── hex_utils.h
│           ├── panic.cpp   # Panic implementation
│           ├── print.cpp   # VGA text mode driver
│           ├── serial.cpp  # UART 16550 driver
│           ├── spinlock.cpp
│           ├── string.cpp  # String utilities
│           └── vga.cpp     # VGA hardware driver
├── targets/
│   └── x86_64/
│       ├── linker.ld    # Linker script (kernel at 1MB, 2GiB identity mapped)
│       └── iso/         # ISO build directory (GRUB structure)
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
- `clang-format` - Code formatting (for `scripts/format.sh`)

### Build Commands

| Command | Description |
|---------|-------------|
| `make build-x86_64` | Build kernel for x86_64 (default target) |
| `make run` | Build and run in QEMU |
| `make run-debug` | Run with debug output (guest errors, unimp) |
| `make run-serial` | Run with serial output to `serial_output.log` |
| `make run-stdio` | Run with serial on stdio (interactive debugging) |
| `make clean` | Remove build artifacts (`build/`, `*.bin`) |
| `make distclean` | Remove all generated files (including `dist/`) |
| `make rebuild` | Clean and rebuild |
| `make verify-tools` | Check if all required tools are available |
| `make help` | Show all available targets |

### Scripts

| Script | Description |
|--------|-------------|
| `scripts/format.sh` | Format all C++ source files using clang-format |
| `scripts/verify_tests.sh` | Verify test output from QEMU serial console |
| `scripts/analyze.sh` | Analysis script |

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

### Code Formatting

- **Style**: LLVM-based with 4-space indentation
- **Column Limit**: 100 characters
- **Braces**: Attached style (`Attach`)
- **Standard**: C++17
- Run `scripts/format.sh` to auto-format all source files

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

The kernel includes self-tests in `kernel_main()`:
- BSS initialization verification
- Memory mapping tests (high memory access)
- Color validation
- Debug macro tests
- Print function tests (64-bit, signed numbers)
- Query function tests (cursor, color)
- Serial output tests (baud rates, signed numbers)
- Hardware detection (CPUID)
- String function tests (including memcpy overlap detection)

Test verification: Run kernel with serial output, then use `scripts/verify_tests.sh test_output.log`

### Error Handling

- **Kernel Panic**: Use `panic()` or `panic_simple()` for fatal errors
- **Assertions**: Use `DEBUG_ASSERT()` for development-time checks
- **No Exceptions**: C++ exceptions are disabled (`-fno-exceptions`)
- **Early Panic**: Direct VGA buffer writes for pre-initialization failures

### Serial Driver Features

- UART 16550 compatible (COM1-COM4)
- Default: COM1 (0x3F8) at 115200 baud
- Error tracking: timeout counts, error codes
- Functions: `serial_write_hex64()`, `serial_write_dec64_signed()`, etc.

### VGA Driver Features

- 80x25 text mode at 0xB8000
- 16 colors (4-bit foreground + 4-bit background)
- SMP-safe via spinlock (atomic operations)
- Functions: `print_hex64()`, `print_dec64_signed()`, cursor management

### Git Workflow

- **Main branch**: `dev` (development)
- **Commit style**: Conventional commits with issue tracking
  - `feat(phase4): Add constants.h for magic numbers [PC005, H002]`
  - `docs(phase4): Translate public API comments to English [MNT001]`
  - `chore(phase4): Update GRUB menu entry name [GRUB001]`

## Key Components

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

### Critical Design Notes

- **BSS Zero-Initialization**: Boot code must zero `.bss` section; verified by `DEBUG_ASSERT` at kernel start
- **2GiB Identity Mapping**: Kernel maps 0x00000000 - 0x7FFFFFFF for safe memory access
- **Freestanding Environment**: No standard library; all functionality implemented from scratch
- **SMP Safety**: VGA driver uses spinlocks for multi-processor safety (CRIT-004)
