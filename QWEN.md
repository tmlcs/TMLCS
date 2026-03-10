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
GLOBEX_OS/
├── docs/                    # Project documentation
│   └── ASSEMBLY_REFERENCE.md  # Inline assembly reference
├── kernels/                 # Kernel implementations by architecture
│   └── x86_64/
│       ├── src/
│       │   ├── arch/        # Architecture-specific code
│       │   │   └── x86_64/
│       │   │       └── boot/   # Boot assembly (Multiboot2, 64-bit entry)
│       │   ├── core/        # Core kernel components
│       │   │   ├── debug/      # Debug macros (DEBUG_PRINT, DEBUG_ASSERT)
│       │   │   └── panic/      # Kernel panic handling
│       │   ├── drivers/     # Hardware drivers
│       │   │   ├── console/  # VGA console driver (print.cpp/h)
│       │   │   ├── serial/   # UART 16550 serial driver
│       │   │   └── vga/      # Low-level VGA hardware access
│       │   ├── kernel/      # Kernel entry point
│       │   │   └── main.cpp  # kernel_main() - OS initialization & tests
│       │   └── lib/         # Kernel libraries
│       │       ├── spinlock/ # SMP spinlock implementation
│       │       ├── string/   # String utilities (memcpy, memset, strlcpy)
│       │       └── utils/    # Utility functions (hex, decimal)
│       ├── tests/           # Test modules
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
│       │   └── test_strlcpy.cpp/h
│       ├── targets/
│       │   └── x86_64/
│       │       ├── linker.ld   # Linker script (1MB load, 2GiB mapped)
│       │       └── iso/        # ISO build directory (GRUB structure)
│       ├── Makefile         # Build system
│       └── dist/            # Build outputs (ISO, kernel.bin)
├── scripts/                 # Utility scripts
│   ├── analyze.sh           # Static analysis (clang-tidy, cppcheck)
│   ├── format.sh            # Code formatting (clang-format)
│   └── verify_tests.sh      # Test verification from serial output
├── .github/                 # GitHub configuration
│   └── workflows/           # CI/CD workflows
├── .clang-format            # Code formatting configuration
├── .editorconfig            # Editor configuration
├── LICENSE                  # BSD 3-Clause License
├── QWEN.md                  # This file - project context
├── QUALITY_ACTION_PLAN.md   # Quality improvement plan
└── SECURITY.md              # Security policy
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

Optional tools for development:
- `clang-format` - Code formatting (use `./scripts/format.sh`)
- `clang-tidy` - Static analysis
- `cppcheck` - Static analysis
- `iwyu` - Include what you use

### Build Commands

All build commands are run from the `kernels/x86_64/` directory:

```bash
cd kernels/x86_64
```

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

### Testing

Tests run automatically in `kernel_main()` and output to serial console:

```bash
# Run kernel and capture serial output
cd kernels/x86_64
make run-serial

# Verify test results
./scripts/verify_tests.sh serial_output.log
```

Test modules cover:
- BSS initialization
- Memory mapping
- Color validation
- Debug macros
- Print functions (64-bit, signed)
- Query functions (cursor, color)
- Serial communication (baud rates, signed numbers)
- String functions (strcpy, strlcpy, memcpy, memmove)

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

Use the provided formatting script:
```bash
./scripts/format.sh
```

Configuration:
- **Style**: LLVM-based with 4-space indentation
- **Column Limit**: 100 characters
- **Braces**: Attached (K&R style)
- **Standard**: C++17

### Debugging

The project uses compile-time debug macros controlled by `DEBUG_ENABLE`:

```cpp
#define DEBUG_ENABLE 1
#include "debug.h"

DEBUG_PRINT("Message");
DEBUG_VAR(myVar, value);      // Prints: "[DEBUG] myVar = 0xCAFEBABE"
DEBUG_ASSERT(condition);      // Breaks if false
DEBUG_LOG("Info message");
DEBUG_PRINTF_HEX("label", val);   // Safe hex output
DEBUG_PRINTF_DEC("label", val);   // Safe decimal output
DEBUG_PRINTLN("Message");         // Message with newline
```

**Security Note**: The variadic `DEBUG_PRINTF(fmt, ...)` macro was removed due to buffer overflow vulnerability. Use type-safe single-argument macros instead.

### Testing Practices

1. **Test Modules**: Each test has a `.h` declaration and `.cpp` implementation
2. **Test Framework**: `test_framework.h` provides common test utilities
3. **Serial Output**: Tests output results to serial for automated verification
4. **Test Markers**: Use `[TEST NAME] PASSED/FAILED` format for verification

### Error Handling

- **Kernel Panic**: Use `panic()` or `panic_simple()` for fatal errors
- **Assertions**: Use `DEBUG_ASSERT()` for development-time checks
- **No Exceptions**: C++ exceptions are disabled (`-fno-exceptions`)
- **Early Panic**: Direct VGA write for pre-initialization failures

### Static Analysis

Run static analysis tools:
```bash
./scripts/analyze.sh
```

Tools used:
- `clang-tidy` - Code style and correctness
- `cppcheck` - Bug detection
- `iwyu` - Include usage optimization

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
- SMP-safe via spinlock

### Serial Driver (`serial.h`/`serial.cpp`)

- UART 16550 compatible
- Default: COM1 (0x3F8) at 115200 baud
- Functions mirror VGA driver for dual-output debugging
- Supports signed 32-bit and 64-bit decimal output
- Error reporting: `serial_get_error_code()`, `serial_has_failed()`, `serial_reinit()`

### String Utilities (`string.h`/`string.cpp`)

- `memcpy()` - Memory copy (non-overlapping)
- `memmove()` - Memory copy with overlap handling
- `memset()` - Memory set
- `memcmp()` - Memory compare
- `strcpy()` - String copy (UNSAFE - no bounds checking)
- `strlcpy()` - Safe bounded string copy (PREFERRED)
- `strlen()` - String length

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

## Troubleshooting

### Common Issues

1. **BSS Test Fails**: Boot code not zeroing `.bss` section
2. **No Serial Output**: Check QEMU serial flags, verify COM1 port
3. **No VGA Output**: Run `print_detect()` before any print functions
4. **Triple Fault**: Check linker script alignment, verify page tables

### Debug Tips

- Use `run-stdio` for interactive serial debugging
- Enable `DEBUG_ENABLE 1` for verbose output
- Check `serial_get_error_code()` for serial issues
- Use `early_panic()` for pre-init error reporting
