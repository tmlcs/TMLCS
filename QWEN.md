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
kernels/x86_64/
├── src/
│   ├── arch/x86_64/
│   │   ├── boot/           # Boot assembly (Multiboot header, 64-bit entry)
│   │   │   ├── header.asm   # Multiboot2 header
│   │   │   ├── main.asm     # Protected mode entry
│   │   │   └── main64.asm   # Long mode entry
│   │   ├── gdt/            # Global Descriptor Table
│   │   ├── idt/            # Interrupt Descriptor Table
│   │   └── include/        # Architecture-specific headers
│   │       ├── atomic.h     # Atomic operations (LOCK-prefixed instructions)
│   │       └── barriers.h   # Memory barriers
│   ├── core/
│   │   ├── constants.h      # VGA, Multiboot, memory constants
│   │   ├── debug/
│   │   │   └── debug.h      # Debug macros (DEBUG_PRINT, DEBUG_ASSERT, etc.)
│   │   ├── panic/
│   │   │   └── panic.h      # Kernel panic functions
│   │   └── stdint/
│   │       └── stdint.h     # Standard integer types
│   ├── drivers/
│   │   ├── console/
│   │   │   └── print.h      # VGA text mode output API (high-level)
│   │   ├── interrupt/
│   │   │   └── interrupts.h # Interrupt handling
│   │   ├── pit/
│   │   │   └── pit.h        # Programmable Interval Timer
│   │   ├── serial/
│   │   │   └── serial.h     # UART 16550 serial port API
│   │   └── vga/
│   │       └── vga.h        # VGA hardware driver (low-level, SMP-safe)
│   ├── kernel/
│   │   └── main.cpp         # kernel_main() - OS initialization & tests
│   ├── lib/
│   │   ├── spinlock/
│   │   │   └── spinlock.h   # Spinlock implementation (SMP-safe)
│   │   ├── string/
│   │   │   └── string.h     # String utilities (memcpy, memset, etc.)
│   │   └── utils/
│   │       ├── decimal_utils.h
│   │       └── hex_utils.h
│   └── memory/
│       └── memory.h         # Memory management
├── targets/
│   └── x86_64/
│       ├── linker.ld        # Linker script (kernel at 1MB, 2GiB identity mapped)
│       └── iso/             # ISO build directory (GRUB structure)
├── tests/                   # Test modules (called from kernel_main)
│   ├── test_bss.*           # BSS initialization verification
│   ├── test_color.*         # Color validation tests
│   ├── test_debug.*         # Debug macro tests
│   ├── test_gdt_idt.*       # GDT/IDT tests
│   ├── test_hardware.*      # CPUID hardware detection
│   ├── test_memory.*        # Memory mapping tests
│   ├── test_memory_manager.*# Memory manager tests
│   ├── test_print.*         # Print function tests (64-bit, signed)
│   ├── test_query.*         # Cursor/color query tests
│   ├── test_serial.*        # Serial baud rate tests
│   ├── test_serial_signed.* # Signed number output tests
│   ├── test_slab.*          # Slab allocator tests
│   ├── test_slab_debug.*    # Slab debugging tests
│   ├── test_spinlock.*      # Spinlock SMP safety tests
│   ├── test_spinlock_smp.*  # SMP spinlock stress tests
│   ├── test_spinlock_stress.*# Spinlock stress tests
│   ├── test_string.*        # String function tests
│   ├── test_string_boundaries.*# String boundary tests
│   └── test_strlcpy.*       # Safe string copy tests
├── Makefile                 # Build system (g++, nasm, ld, grub-mkrescue)
└── dist/                    # Build outputs (ISO, kernel.bin)
```

## Memory Layout

| Section | Address | Size | Permissions | Purpose |
|---------|---------|------|-------------|---------|
| Kernel Load | 0x100000 | Variable | R+X | Kernel binary loaded by GRUB |
| Mapped Memory | 0x00000000 - 0x7FFFFFFF | 2GiB | R+W | Identity-mapped region |
| VGA Buffer | 0xB8000 | 4KB | R+W | Text mode display (80x25) |
| Page Tables | .boot.data | 64KB+ | R+W | Boot-time page tables |

- **Page Size**: 4KB standard, 2MB large pages
- **BSS**: Zero-initialized by boot code (verified at kernel entry)

## Building and Running

### Prerequisites

Required tools (verify with `make verify-tools`):
- `g++` - C++ compiler (freestanding mode)
- `nasm` - Assembly compiler
- `ld` - GNU linker
- `grub-mkrescue` - GRUB ISO creator
- `qemu-system-x86_64` - QEMU emulator
- `clang-format` - Code formatting
- `clang-tidy` - Static analysis (optional)
- `cppcheck` - Static analysis (optional)

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
| `make analyze` | Run static analysis (clang-tidy + cppcheck) |
| `make analyze-verbose` | Run static analysis with output logging |

### Compiler Flags

The kernel uses strict freestanding compilation:
```makefile
CFLAGS := -ffreestanding -fno-exceptions -fno-rtti \
          -fno-stack-protector -nostdlib -nostartfiles \
          -O2 -Wall -Wextra -Wpedantic -Werror -Wuninitialized \
          -I src/core \
          -I src/core/panic \
          -I src/core/debug \
          -I src/drivers/console \
          -I src/drivers/vga \
          -I src/drivers/serial \
          -I src/lib/string \
          -I src/lib/spinlock \
          -I src/lib/utils \
          -I src/arch/x86_64/include
```

### QEMU Flags

| Mode | Flags |
|------|-------|
| Normal | `-cdrom $(ISO_FILE)` |
| Debug | `-cdrom $(ISO_FILE) -d guest_errors,unimp -no-reboot -no-shutdown` |
| Serial (file) | `-cdrom $(ISO_FILE) -serial file:serial_output.log -no-reboot -no-shutdown` |
| Serial (stdio) | `-cdrom $(ISO_FILE) -serial stdio -no-reboot -no-shutdown` |

## Development Conventions

### Code Structure

1. **Interface/Implementation Split**:
   - Headers (`.h`) in `src/` subdirectories - Public API with C linkage (`extern "C"`)
   - Implementation (`.cpp`) alongside headers - C++ implementation
   - Assembly (`.asm`) in `src/arch/x86_64/boot/` - Boot code

2. **Header Guards**: All headers use `#ifndef` guards with `extern "C"` for C++ compatibility

3. **Naming Conventions**:
   - Constants: `UPPER_SNAKE_CASE` (e.g., `VGA_BUFFER_ADDRESS`, `SERIAL_COM1`)
   - Types: `PascalCase` with `_t` suffix (e.g., `PrintColor_t`, `SerialState_t`)
   - Functions: `snake_case` with module prefix (e.g., `print_set_color`, `serial_write_str`)
   - Variables: `snake_case` (e.g., `debug_test_value`, `test_cursor_col`)

4. **Documentation Style**:
   - Doxygen-style comments for public API
   - Inline comments for complex logic
   - Security notes for critical code paths
   - **All comments and documentation must be in English** (see `docs/DOCUMENTATION_STYLE_GUIDE.md`)

### Debugging

The project uses compile-time debug macros controlled by `DEBUG_ENABLE`:

```cpp
#define DEBUG_ENABLE 1
#include "debug.h"

DEBUG_PRINT("Message");
DEBUG_VAR(myVar, value);      // Prints: "[DEBUG] myVar = 0xCAFEBABE"
DEBUG_ASSERT(condition);      // Breaks if false
DEBUG_LOG("Info message");
DEBUG_PRINTF_HEX("label", val);   // Safe single-argument hex output
DEBUG_PRINTF_DEC("label", val);   // Safe single-argument decimal output
DEBUG_BREAK();                // Print file:line location
```

**Security Note**: The variadic `DEBUG_PRINTF(fmt, ...)` was removed due to buffer overflow vulnerability. Use type-safe single-argument macros instead.

### Testing Practices

The kernel includes comprehensive self-tests in `kernel_main()`:

1. **BSS Initialization** - Verifies boot code zeroes `.bss` section
2. **Memory Mapping** - Tests 2GiB identity-mapped region
3. **Color Validation** - VGA color attribute tests
4. **Debug Macros** - Tests all debug output functions
5. **Print Functions** - 64-bit hex/decimal, signed number output
6. **Query Functions** - Cursor position, color attribute queries
7. **Serial Tests** - Baud rate detection, null pointer handling
8. **String Functions** - `memcpy`, `memset`, `strcmp`, `strlen`
9. **Safe String Copy** - `strlcpy` overflow prevention
10. **Spinlock** - SMP safety, acquire/release, interrupt handling
11. **Hardware Info** - CPUID detection

Test verification script:
```bash
# Run kernel and capture serial output
make run-serial
# Verify test results
./scripts/verify_tests.sh serial_output.log
```

### SMP Safety

The kernel is designed for SMP (Symmetric Multi-Processing) environments:

- **Spinlocks**: `spinlock_t` with `LOCK CMPXCHG` instructions
- **Atomic Operations**: `atomic_*` functions in `atomic.h`
- **Memory Barriers**: `rmb()`, `wmb()`, `mb()` for ordering
- **VGA Driver**: SMP-safe via `vga_lock()` / `vga_unlock()`
- **Serial Driver**: State checks use `rmb()`, but concurrent writes may interleave

**Interrupt Safety**: Spinlocks ALWAYS disable interrupts during acquire to prevent nested deadlock (CRIT-004).

### Error Handling

- **Kernel Panic**: Use `panic()` or `panic_simple()` for fatal errors
- **Early Panic**: `early_panic()` for pre-driver failures (direct VGA write)
- **Assertions**: Use `DEBUG_ASSERT()` for development-time checks
- **No Exceptions**: C++ exceptions are disabled (`-fno-exceptions`)
- **Error Codes**: Serial driver returns error codes via `serial_get_error_code()`

### Code Formatting

Use the provided script to format all C++ files:
```bash
./scripts/format.sh
```

Configuration:
- `.clang-format` - LLVM-based style, 4-space indent, 100 column limit
- `.editorconfig` - Consistent indentation (4 spaces for C++, tabs for assembly/Makefile)

### Static Analysis

Run static analysis locally:
```bash
# Install tools
sudo apt-get install clang-tidy cppcheck

# Run analysis
./scripts/analyze.sh
```

See `docs/CICD_GUIDE.md` for detailed static analysis documentation.

### Git Workflow

- **Main branch**: `dev` (development)
- **Commit style**: Conventional commits with issue tracking
  - `feat(phase4): Add constants.h for magic numbers [PC005, H002]`
  - `docs(phase4): Translate public API comments to English [MNT001]`
  - `chore(phase4): Update GRUB menu entry name [GRUB001]`

## Key Components

### VGA Text Mode Driver (`vga.h`/`vga.cpp` + `print.h`/`print.cpp`)

**Separation of Concerns**:
- `vga.h/vga.cpp`: Low-level hardware access, cursor, colors (SMP-safe via spinlock)
- `print.h/print.cpp`: High-level formatting (decimal, hex, strings)

**Features**:
- 80x25 text mode at 0xB8000
- 16 colors (4-bit foreground + 4-bit background)
- Hardware cursor control
- Atomic operations for multi-step updates

**Functions**: `print_str()`, `print_char()`, `print_hex()`, `print_dec()`, `print_hex64()`, `print_dec_signed()`, `print_dec64_signed()`

**Query Functions**: `print_get_cursor()`, `print_get_color()`, `print_set_cursor()`, `print_set_color()`

### Serial Driver (`serial.h`/`serial.cpp`)

**Hardware**: UART 16550 compatible

**Default**: COM1 (0x3F8) at 115200 baud

**Features**:
- Mirrors VGA driver API for dual-output debugging
- Supports signed 32-bit and 64-bit decimal output
- Error reporting via `serial_get_error_code()`, `serial_get_timeout_count()`
- Recovery via `serial_reinit()` after timeout

**SMP Threading Model**:
- State checks use `rmb()` for visibility
- NOT atomic for concurrent writes (may interleave characters)
- Use external spinlock for full SMP safety

### Boot Process

1. **Multiboot2 Header** (`header.asm`) - GRUB identification, checksum validation
2. **Boot Assembly** (`main.asm`, `main64.asm`) - Protected mode to long mode transition, page table setup
3. **Linker Script** (`linker.ld`) - Memory layout with explicit segment permissions
4. **Kernel Entry** (`kernel_main()`) - C++ initialization and tests

**Memory Sections**:

| Section | Purpose | Permissions |
|---------|---------|-------------|
| `.boot` | Multiboot header | R+X |
| `.text` | Code | R+X |
| `.rodata` | Constants, strings | R |
| `.data` | Initialized globals | R+W |
| `.bss` | Zero-initialized globals | R+W |
| `.boot.data` | Page tables, boot stack | R+W (NOBITS) |

### Spinlock API (`spinlock.h`)

**Implementation**: Ticket lock with `LOCK CMPXCHG`

**Usage**:
```cpp
spinlock_t my_lock = SPINLOCK_INIT;

spinlock_acquire(&my_lock);
// ... critical section ...
spinlock_release(&my_lock);
```

**Interrupt Safety**: ALWAYS disables interrupts during acquire to prevent nested deadlock (CRIT-004 fix).

**VGA Lock**: Global `vga_lock()` / `vga_unlock()` for protecting VGA operations.

### Atomic Operations (`atomic.h`)

**32-bit**: `atomic_inc32()`, `atomic_dec32()`, `atomic_add32()`, `atomic_load32()`, `atomic_store32()`

**64-bit**: `atomic_inc64()`, `atomic_dec64()`, `atomic_add64()`, `atomic_load64()`, `atomic_store64()`

**CAS**: `atomic_compare_exchange32()`, `atomic_compare_exchange64()`

**Memory Ordering**: Default `__ATOMIC_SEQ_CST` (sequential consistency), relaxed variants available for counters.

## CI/CD Pipeline

GitHub Actions runs automated checks on every push and pull request:

| Job | Description | Required |
|-----|-------------|----------|
| `build` | Compiles the kernel | ✅ Yes |
| `test` | Runs tests in QEMU | ✅ Yes |
| `static-analysis` | Runs clang-tidy and cppcheck | ⚠️ Advisory |
| `code-style` | Verifies code formatting | ✅ Yes |
| `summary` | Aggregates all results | - |

See `docs/CICD_GUIDE.md` for detailed CI/CD documentation.

## Troubleshooting

### Common Issues

1. **BSS not zeroed**: Boot code bug - check `.boot.data` section initialization
2. **Serial timeout**: Hardware not responding - verify QEMU serial flags
3. **VGA not detected**: Check 0xB8000 accessibility in emulator
4. **Spinlock deadlock**: Ensure interrupts are restored after release
5. **Linker warning RWX**: Use explicit `PHDRS` with correct flags (5=R+X, 6=R+W)

### Debug Output

Enable serial output for debugging:
```bash
make run-stdio  # Serial on stdout (interactive)
# or
make run-serial && cat serial_output.log
```

### Build Artifacts

| File | Location | Purpose |
|------|----------|---------|
| `kernel.bin` | `dist/x86_64/` | Raw kernel binary |
| `kernel.iso` | `dist/x86_64/` | Bootable ISO for QEMU |
| `serial_output.log` | Project root | Serial console capture |
| `build/` | Project root | Object files |

## Documentation

- `docs/CICD_GUIDE.md` - CI/CD and static analysis guide
- `docs/DOCUMENTATION_STYLE_GUIDE.md` - Documentation standards (English-only policy)
- `.github/workflows/ci-cd.yml` - GitHub Actions workflow definition

## References

- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html)
- [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [OSDev Wiki](https://wiki.osdev.org/)
