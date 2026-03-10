# ==========================================
# Toolchain Configuration
# ==========================================
CC := g++
AS := nasm
LD := ld
GRUB := grub-mkrescue
QEMU := qemu-system-x86_64

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
          -O2 -Wall -Wextra -Wpedantic -Werror \
          -I src/core \
          -I src/core/panic \
          -I src/core/debug \
          -I src/drivers/console \
          -I src/drivers/vga \
          -I src/drivers/serial \
          -I src/lib/string \
          -I src/lib/spinlock \
          -I src/lib/utils

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

# Boot assembly
BOOT_SRCS := $(shell find src/arch/x86_64 -name '*.asm')
BOOT_OBJS := $(patsubst src/arch/x86_64/%.asm,$(BUILD_DIR)/arch/x86_64/%.o,$(BOOT_SRCS))

# Drivers
DRIVER_SRCS := $(shell find src/drivers -name '*.cpp')
DRIVER_OBJS := $(patsubst src/drivers/%.cpp,$(BUILD_DIR)/drivers/%.o,$(DRIVER_SRCS))

# Library
LIB_SRCS := $(shell find src/lib -name '*.cpp')
LIB_OBJS := $(patsubst src/lib/%.cpp,$(BUILD_DIR)/lib/%.o,$(LIB_SRCS))

# Core
CORE_SRCS := $(shell find src/core -name '*.cpp')
CORE_OBJS := $(patsubst src/core/%.cpp,$(BUILD_DIR)/core/%.o,$(CORE_SRCS))

# Tests
TEST_SRCS := $(shell find tests -name '*.cpp')
TEST_OBJS := $(patsubst tests/%.cpp,$(BUILD_DIR)/tests/%.o,$(TEST_SRCS))

# All object files
ALL_OBJS := $(KERNEL_OBJS) $(BOOT_OBJS) $(DRIVER_OBJS) $(LIB_OBJS) $(CORE_OBJS) $(TEST_OBJS)

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

# Assembly files
$(BUILD_DIR)/%.o: src/%.asm
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
	@echo "Utility targets:"
	@echo "  verify-tools  - Check if all required tools are available"
	@echo "  help          - Show this help message"
