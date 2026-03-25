# ==========================================
# Toolchain Configuration
# ==========================================
CC := g++
AS := nasm
LD := ld
GRUB := grub-mkrescue
QEMU := qemu-system-x86_64

# ==========================================
# Project Root
# ==========================================
# Get the project root directory (two levels up from kernels/x86_64)
PROJECT_ROOT := $(shell dirname $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))

# ==========================================
# Directories and Outputs
# ==========================================
ISO_DIR := dist/x86_64
BUILD_DIR := build
KERNEL_BIN := $(ISO_DIR)/kernel.bin
ISO_FILE := $(ISO_DIR)/kernel.iso

# ==========================================
# QEMU Flags
# ==========================================
QEMU_FLAGS := -cdrom $(ISO_FILE)
QEMU_DEBUG_FLAGS := -cdrom $(ISO_FILE) -d guest_errors,unimp -no-reboot -no-shutdown
QEMU_SERIAL_FLAGS := -cdrom $(ISO_FILE) -serial file:serial_output.log -display none -no-reboot -no-shutdown
QEMU_STDIO_FLAGS := -cdrom $(ISO_FILE) -serial stdio -no-reboot -no-shutdown

# ==========================================
# Compiler Flags - CRITICAL for kernel development
# ==========================================
# Include paths for all subdirectories
CFLAGS := -ffreestanding -fno-exceptions -fno-rtti \
          -fno-stack-protector -nostdlib -nostartfiles \
          -mno-sse -mno-sse2 -mno-mmx -mno-avx -mno-red-zone \
          -O2 -Wall -Wextra -Wpedantic -Werror -Wuninitialized \
          -I src/core \
          -I src/core/panic \
          -I src/core/debug \
          -I src/core/stdint \
          -I src/core/log \
          -I src/drivers/console \
          -I src/drivers/vga \
          -I src/drivers/serial \
          -I src/drivers/pit \
          -I src/drivers/interrupts \
          -I src/lib/string \
          -I src/lib/spinlock \
          -I src/lib/utils \
          -I src/arch/x86_64/include \
          -I src/arch/x86_64/idt \
          -I src/arch/x86_64/gdt \
          -I src/memory

# ==========================================
# Log Level Configuration
# ==========================================
# Set LOG_MAX_LEVEL for compile-time log filtering:
#   - LOG_LEVEL_DEBUG (0): All logs (verbose)
#   - LOG_LEVEL_INFO (1):  Info and above (default)
#   - LOG_LEVEL_WARN (2):  Warnings and errors only
#   - LOG_LEVEL_ERROR (3): Errors only
#   - LOG_LEVEL_PANIC (4): Panics only (silent)
#
# Usage: make LOG_LEVEL=0   # Debug build
#        make LOG_LEVEL=1   # Info build (default)
#        make LOG_LEVEL=2   # Warn build
#        make LOG_LEVEL=3   # Error build
# ==========================================
LOG_LEVEL ?= 1
CFLAGS += -DLOG_MAX_LEVEL=$(LOG_LEVEL)

# ==========================================
# Assembly Flags
# ==========================================
ASFLAGS := -f elf64 -g

# ==========================================
# Source Files Discovery
# ==========================================
# Kernel entry point
KERNEL_SRCS := $(shell find src/kernel -name '*.cpp')
KERNEL_OBJS := $(patsubst src/kernel/%.cpp,$(BUILD_DIR)/kernel/%.o,$(KERNEL_SRCS))

# Boot assembly (only from boot directory)
BOOT_SRCS := $(shell find src/arch/x86_64/boot -name '*.asm')
BOOT_OBJS := $(patsubst src/arch/x86_64/%.asm,$(BUILD_DIR)/arch/x86_64/%.o,$(BOOT_SRCS))

# IDT assembly (put in idt/asm subdirectory to avoid conflicts)
IDT_ASM_SRCS := $(shell find src/arch/x86_64/idt -name '*.asm')
IDT_ASM_OBJS := $(patsubst src/arch/x86_64/idt/%.asm,$(BUILD_DIR)/arch/x86_64/idt/asm/%.o,$(IDT_ASM_SRCS))

# GDT assembly (put in gdt/asm subdirectory to avoid conflicts)
GDT_ASM_SRCS := $(shell find src/arch/x86_64/gdt -name '*.asm')
GDT_ASM_OBJS := $(patsubst src/arch/x86_64/gdt/%.asm,$(BUILD_DIR)/arch/x86_64/gdt/asm/%.o,$(GDT_ASM_SRCS))

# Drivers
DRIVER_SRCS := $(shell find src/drivers -name '*.cpp')
DRIVER_OBJS := $(patsubst src/drivers/%.cpp,$(BUILD_DIR)/drivers/%.o,$(DRIVER_SRCS))

# Library
LIB_SRCS := $(shell find src/lib -name '*.cpp')
LIB_OBJS := $(patsubst src/lib/%.cpp,$(BUILD_DIR)/lib/%.o,$(LIB_SRCS))

# Core
CORE_SRCS := $(shell find src/core -name '*.cpp')
CORE_OBJS := $(patsubst src/core/%.cpp,$(BUILD_DIR)/core/%.o,$(CORE_SRCS))

# Memory
MEMORY_SRCS := $(shell find src/memory -name '*.cpp')
MEMORY_OBJS := $(patsubst src/memory/%.cpp,$(BUILD_DIR)/memory/%.o,$(MEMORY_SRCS))

# Arch-specific C++ (GDT, IDT, etc.)
ARCH_SRCS := $(shell find src/arch/x86_64 -name '*.cpp')
ARCH_OBJS := $(patsubst src/arch/x86_64/%.cpp,$(BUILD_DIR)/arch/x86_64/%.o,$(ARCH_SRCS))

# Tests
TEST_SRCS := $(shell find tests -name '*.cpp')
TEST_OBJS := $(patsubst tests/%.cpp,$(BUILD_DIR)/tests/%.o,$(TEST_SRCS))

# All object files
ALL_OBJS := $(KERNEL_OBJS) $(BOOT_OBJS) $(IDT_ASM_OBJS) $(GDT_ASM_OBJS) $(DRIVER_OBJS) $(LIB_OBJS) $(CORE_OBJS) $(MEMORY_OBJS) $(ARCH_OBJS) $(TEST_OBJS)

# ==========================================
# Compilation Rules - Generic
# ==========================================
# C++ files
$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Test files
$(BUILD_DIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly files (generic rule)
$(BUILD_DIR)/%.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# IDT assembly files (special rule for asm subdirectory)
$(BUILD_DIR)/arch/x86_64/idt/asm/%.o: src/arch/x86_64/idt/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# GDT assembly files (special rule for asm subdirectory)
$(BUILD_DIR)/arch/x86_64/gdt/asm/%.o: src/arch/x86_64/gdt/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# ==========================================
# Build Targets
# ==========================================
.PHONY: build-x86_64
build-x86_64: $(ALL_OBJS)
	@mkdir -p $(ISO_DIR)
	$(LD) -n -o $(KERNEL_BIN) -T targets/x86_64/linker.ld $(ALL_OBJS)
	cp $(KERNEL_BIN) targets/x86_64/iso/boot/kernel.bin
	$(GRUB) /usr/lib/grub/i386-pc -o $(ISO_FILE) targets/x86_64/iso

# Emulation
.PHONY: run
run: build-x86_64
	qemu-system-x86_64 -cdrom $(ISO_FILE)

.PHONY: run-debug
run-debug: build-x86_64
	qemu-system-x86_64 -cdrom $(ISO_FILE) -d guest_errors,unimp -no-reboot -no-shutdown

.PHONY: run-serial
run-serial: build-x86_64
	qemu-system-x86_64 -cdrom $(ISO_FILE) -serial file:serial_output.log -no-reboot -no-shutdown
	@echo "=== Serial Output ==="
	@cat serial_output.log

.PHONY: run-stdio
run-stdio: build-x86_64
	$(QEMU) $(QEMU_STDIO_FLAGS)

# ==========================================
# Cleaning
# ==========================================
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(KERNEL_BIN)
	rm -f serial_output.log

.PHONY: distclean
distclean: clean
	rm -rf $(ISO_DIR)
	rm -f $(ISO_FILE)

.PHONY: rebuild
rebuild: clean build-x86_64

# ==========================================
# Static Analysis
# ==========================================
.PHONY: analyze
analyze:
	@echo "Running static analysis..."
	@cd $(PROJECT_ROOT) && ./scripts/analyze.sh

.PHONY: analyze-verbose
analyze-verbose:
	@echo "Running static analysis (verbose)..."
	@cd $(PROJECT_ROOT) && ./scripts/analyze.sh 2>&1 | tee analyze.log

# ==========================================
# Tool Verification
# ==========================================
.PHONY: verify-tools
verify-tools:
	@echo "Checking required tools..."
	@command -v $(CC) >/dev/null 2>&1 || { echo "ERROR: $(CC) not found"; exit 1; }
	@command -v $(AS) >/dev/null 2>&1 || { echo "ERROR: $(AS) not found"; exit 1; }
	@command -v $(LD) >/dev/null 2>&1 || { echo "ERROR: $(LD) not found"; exit 1; }
	@command -v $(GRUB) >/dev/null 2>&1 || { echo "ERROR: $(GRUB) not found"; exit 1; }
	@command -v $(QEMU) >/dev/null 2>&1 || { echo "ERROR: $(QEMU) not found"; exit 1; }
	@echo "All tools available:"
	@echo "  CC:   $(CC)"
	@echo "  AS:   $(AS)"
	@echo "  LD:   $(LD)"
	@echo "  GRUB: $(GRUB)"
	@echo "  QEMU: $(QEMU)"

# ==========================================
# Help
# ==========================================
.PHONY: help
help:
	@echo "GLOBEX_OS Build System"
	@echo "======================"
	@echo ""
	@echo "Build targets:"
	@echo "  build-x86_64  - Build kernel for x86_64 (default)"
	@echo "  clean         - Remove build artifacts"
	@echo "  distclean     - Remove all generated files"
	@echo "  rebuild       - Clean and rebuild"
	@echo ""
	@echo "Emulation targets:"
	@echo "  run           - Run in QEMU (default)"
	@echo "  run-debug     - Run with debug output"
	@echo "  run-serial    - Run with serial output to file"
	@echo "  run-stdio     - Run with serial on stdio"
	@echo ""
	@echo "Analysis targets:"
	@echo "  analyze       - Run static analysis (clang-tidy + cppcheck)"
	@echo "  analyze-verbose - Run static analysis with output logging"
	@echo ""
	@echo "Utility targets:"
	@echo "  verify-tools  - Check if all required tools are available"
	@echo "  help          - Show this help message"
