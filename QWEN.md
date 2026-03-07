# GLOBEX_OS - 64-bit Kernel Development

## Project Overview

**GLOBEX_OS** is a hobby operating system kernel written from scratch in **C++** and **x86_64 assembly**. This project implements a 64-bit kernel that boots via Multiboot, sets up paging, and enters long mode.

- **Architecture:** x86_64 (AMD64)
- **Languages:** C++ (freestanding), NASM Assembly
- **Boot Protocol:** Multiboot2
- **Based on:** [YouTube tutorial series](https://www.youtube.com/playlist?list=PLZQftyCk7_SeZRitx5MjBKzTtvk0pHMtp) by David Callanan
- **License:** BSD 3-Clause

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      GLOBEX_OS Kernel                        │
├─────────────────────────────────────────────────────────────┤
│  Application Layer                                           │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  kernel/main.cpp - kernel_main() entry point        │    │
│  └─────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│  Hardware Abstraction Layer                                  │
│  ┌─────────────────┐  ┌─────────────────┐                   │
│  │  VGA Text Mode  │  │  UART Serial    │                   │
│  │  (print.h/cpp)  │  │  (serial.h/cpp) │                   │
│  │  - 80x25 text   │  │  - COM1 115200  │                   │
│  │  - Colors       │  │  - Debug output │                   │
│  └─────────────────┘  └─────────────────┘                   │
├─────────────────────────────────────────────────────────────┤
│  Boot Section (x86_64/boot/)                                 │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  header.asm  - Multiboot2 header                    │    │
│  │  main.asm    - 32-bit protected mode, paging setup  │    │
│  │  main64.asm  - 64-bit long mode entry               │    │
│  └─────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│  Hardware                                                    │
│  VGA Buffer @ 0xB8000  │  UART COM1 @ 0x3F8                 │
└─────────────────────────────────────────────────────────────┘
```

---

## Building and Running

### Prerequisites

- **Docker** - Build environment containerization
- **QEMU** - x86_64 system emulation (`qemu-system-x86_64`)
- **Text Editor** - VS Code or similar

### Setup Build Environment

```bash
# Build the Docker image (one-time setup)
docker build buildenv -t myos-buildenv
```

### Enter Build Environment

```bash
# Linux/macOS
docker run --rm -it -v "$(pwd)":/root/env myos-buildenv

# Windows (PowerShell)
docker run --rm -it -v "${pwd}:/root/env" myos-buildenv

# Windows (CMD)
docker run --rm -it -v "%cd%":/root/env myos-buildenv
```

### Build Targets

```bash
# Build the kernel ISO
make build-x86_64

# Run in QEMU
make run

# Run with debug output (guest errors, unimplemented)
make run-debug

# Run with serial output to file
make run-serial
# Output: serial_output.log

# Run with serial on stdio (interactive)
make run-stdio
```

### Emulate Manually

```bash
qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso
```

If QEMU fails to find BIOS files:
```bash
# Windows
qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso -L "C:\Program Files\qemu"

# Linux
qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso -L /usr/share/qemu/
```

---

## Directory Structure

```
GLOBEX_OS/
├── 0_008_x64/                    # Main kernel source
│   ├── Makefile                  # Build configuration
│   ├── README.md                 # Tutorial reference
│   ├── src/
│   │   ├── intf/                 # Public interfaces (headers)
│   │   │   ├── debug.h           # Debug macros (serial-based)
│   │   │   ├── print.h           # VGA text mode API
│   │   │   └── serial.h          # UART 16550 driver API
│   │   └── impl/                 # Implementations
│   │       ├── kernel/
│   │       │   └── main.cpp      # kernel_main() entry point
│   │       └── x86_64/
│   │           ├── boot/
│   │           │   ├── header.asm    # Multiboot2 header
│   │           │   ├── main.asm      # 32-bit boot code
│   │           │   └── main64.asm    # 64-bit entry point
│   │           ├── print.cpp     # VGA text mode driver
│   │           └── serial.cpp    # UART serial driver
│   └── targets/x86_64/
│       ├── linker.ld             # Linker script (kernel @ 1MB)
│       └── iso/
│           └── boot/
│               └── grub.cfg      # GRUB configuration
├── LICENSE                       # BSD 3-Clause
├── README.md                     # Developer profile
└── SECURITY.md                   # Security policy
```

---

## Technical Details

### Boot Process

1. **Multiboot2 Header** (`header.asm`)
   - Magic: `0xe85250d6`
   - Architecture: Protected mode (0)
   - Loaded at 1MB physical address

2. **32-bit Protected Mode** (`main.asm`)
   - Validate Multiboot magic number
   - Check CPUID support
   - Verify Long Mode support
   - Setup 4-level page tables (2MiB pages)
   - Enable PAE and Long Mode
   - Jump to 64-bit code segment

3. **64-bit Long Mode** (`main64.asm`)
   - Zero-initialize BSS section
   - Call `kernel_main()` in C++

### Memory Layout

```
0x00000000 - 0x000FFFFF : Real Mode / BIOS Areas
0x00100000 - 0x00AFFFFF : Kernel Code/Data (loaded at 1MB)
0x00B80000 - 0x00B8FFFF : VGA Text Buffer (80x25 text mode)
0x00B90000 - ...        : Available RAM
```

### VGA Text Mode

- **Buffer:** 80 columns × 25 rows = 2000 characters
- **Address:** `0xB8000` (memory-mapped I/O)
- **Format:** Each character = 2 bytes (ASCII + attribute)
- **Colors:** 16 foreground × 16 background (4-bit each)

### Serial Console (UART 16550)

- **Port:** COM1 (`0x3F8`)
- **Baud Rate:** 115200
- **Configuration:** 8N1 (8 data, no parity, 1 stop)
- **Use Case:** Debug output, logging, interactive console

### Compiler Configuration

```makefile
CFLAGS := -ffreestanding        # No hosted environment
          -fno-exceptions       # No C++ exceptions
          -fno-rtti             # No runtime type info
          -fno-stack-protector  # No stack canaries
          -nostdlib             # No standard library
          -nostartfiles         # No startup files
          -O2                   # Optimization level 2
          -Wall -Wextra -Wpedantic  # Strict warnings
          -Werror               # Warnings as errors
```

---

## Debugging

### Serial Debug Output

Enable debug macros in `debug.h`:

```c
#define DEBUG_ENABLE 1
```

Use debug macros:

```c
DEBUG_PRINT("Value: ");
DEBUG_VAR(my_var, value);
DEBUG_BREAK();
```

### QEMU Debug Options

```bash
# Log guest errors and unimplemented features
make run-debug

# Serial output to file
make run-serial
cat serial_output.log

# Serial on stdio (interactive)
make run-stdio
```

### Common Debug Scenarios

| Symptom | Possible Cause |
|---------|----------------|
| Triple fault | Invalid GDT/IDT, stack overflow |
| No output | VGA not detected, serial not initialized |
| "ERR: X" on screen | Boot error (M=Multiboot, C=CPUID, L=Long Mode) |

---

## Development Conventions

### Code Style

- **Naming:** `snake_case` for functions/variables, `PascalCase` for types/enums
- **Headers:** Include guards with `#ifndef HEADER_NAME_H`
- **Comments:** Doxygen-style for public APIs, inline for complex logic
- **Extern C:** All headers use `extern "C"` for C/C++ compatibility

### Kernel Constraints

- **No standard library** - All functionality must be implemented from scratch
- **No dynamic allocation** - No heap/malloc in early kernel
- **No exceptions/RTTI** - Disabled at compile time
- **Volatile for MMIO** - Hardware registers must be `volatile`
- **Noreturn for kernel_main** - Kernel never returns

### Safety Practices

1. **Null checks** - Validate pointers before dereferencing
2. **Bounds checking** - Validate array indices
3. **Volatile access** - Use `volatile` for hardware MMIO
4. **Memory barriers** - Prevent reordering with `__asm__ volatile`

---

## Key Files Reference

| File | Purpose |
|------|---------|
| `src/intf/print.h` | VGA text mode API |
| `src/intf/serial.h` | UART driver API with register definitions |
| `src/intf/debug.h` | Debug macros (conditional compilation) |
| `src/impl/kernel/main.cpp` | Kernel entry point |
| `src/impl/x86_64/boot/header.asm` | Multiboot2 header |
| `src/impl/x86_64/boot/main.asm` | 32-bit boot sequence |
| `targets/x86_64/linker.ld` | Linker script (sections, memory layout) |
| `Makefile` | Build system (Docker, QEMU integration) |

---

## Resources

- **Tutorial Series:** [YouTube Playlist](https://www.youtube.com/playlist?list=PLZQftyCk7_SeZRitx5MjBKzTtvk0pHMtp)
- **Source Repository:** [GitHub](https://github.com/davidcallanan/yt-os-series)
- **Pre-built ISOs:** [os-series-isos](https://github.com/davidcallanan/os-series-isos)
- **Support:** [Patreon](http://patreon.com/codepulse)
