# ==========================================
# Toolchain Configuration
# ==========================================
CC := g++
AS := nasm
LD := ld
GRUB := grub-mkrescue
QEMU := qemu-system-x86_64

# ==========================================
# Directorios y Outputs
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
QEMU_STDIO_FLAGS := -cdrom $(ISO_FILE) -serial stdio -display none -no-reboot -no-shutdown

# ==========================================
# Compiler Flags - CRÍTICO para kernel development
# ==========================================
# -ffreestanding: No asumir entorno hosted (no stdlib)
# -fno-exceptions: Deshabilitar exceptions (no runtime en kernel)
# -fno-rtti: Deshabilitar RTTI (requiere runtime)
# -fno-stack-protector: Stack protector requiere runtime
# -nostdlib: No linkear contra standard library
# -nostartfiles: No usar startup files
# -O2: Optimización nivel 2 (balance speed/debug)
# -Wall -Wextra -Wpedantic: Warnings máximos
# -Werror: Tratar warnings como errores
CFLAGS := -ffreestanding -fno-exceptions -fno-rtti \
          -fno-stack-protector -nostdlib -nostartfiles \
          -O2 -Wall -Wextra -Wpedantic -Werror \
          -I src/intf

# ==========================================
# Assembly Flags
# ==========================================
ASFLAGS := -f elf64 -g

# ==========================================
# Source Files Discovery
# ==========================================
kernel_source_files := $(shell find src/impl/kernel -name '*.cpp')
kernel_object_files := $(patsubst src/impl/kernel/%.cpp, $(BUILD_DIR)/kernel/%.o, $(kernel_source_files))

x86_64_cpp_source_files := $(shell find src/impl/x86_64 -name '*.cpp')
x86_64_cpp_object_files := $(patsubst src/impl/x86_64/%.cpp, $(BUILD_DIR)/x86_64/%.o, $(x86_64_cpp_source_files))

x86_64_asm_source_files := $(shell find src/impl/x86_64 -name '*.asm')
x86_64_asm_object_files := $(patsubst src/impl/x86_64/%.asm, $(BUILD_DIR)/x86_64/%.o, $(x86_64_asm_source_files))

x86_64_object_files := $(x86_64_cpp_object_files) $(x86_64_asm_object_files)

# ==========================================
# Reglas de Compilación
# ==========================================
$(kernel_object_files): $(BUILD_DIR)/kernel/%.o : src/impl/kernel/%.cpp
	mkdir -p $(dir $@) && \
	$(CC) -c $(CFLAGS) $< -o $@

$(x86_64_cpp_object_files): $(BUILD_DIR)/x86_64/%.o : src/impl/x86_64/%.cpp
	mkdir -p $(dir $@) && \
	$(CC) -c $(CFLAGS) $< -o $@

$(x86_64_asm_object_files): $(BUILD_DIR)/x86_64/%.o : src/impl/x86_64/%.asm
	mkdir -p $(dir $@) && \
	$(AS) -f elf64 $< -o $@

# Construcción del kernel
.PHONY: build-x86_64
build-x86_64: $(kernel_object_files) $(x86_64_object_files)
	mkdir -p $(ISO_DIR) && \
	$(LD) -n -o $(KERNEL_BIN) -T targets/x86_64/linker.ld $(kernel_object_files) $(x86_64_object_files) && \
	cp $(KERNEL_BIN) targets/x86_64/iso/boot/kernel.bin && \
	$(GRUB) /usr/lib/grub/i386-pc -o $(ISO_FILE) targets/x86_64/iso

# Emulación
.PHONY: run
run: build-x86_64
	qemu-system-x86_64 -cdrom $(ISO_FILE)

.PHONY: run-debug
run-debug: build-x86_64
	qemu-system-x86_64 -cdrom $(ISO_FILE) -d guest_errors,unimp -no-reboot -no-shutdown

# Emulación con output serial a archivo
.PHONY: run-serial
run-serial: build-x86_64
	qemu-system-x86_64 -cdrom $(ISO_FILE) -serial file:serial_output.log -display none -no-reboot -no-shutdown
	@echo "=== Serial Output ==="
	@cat serial_output.log

# Emulación con serial en stdio (para debugging interactivo)
.PHONY: run-stdio
run-stdio: build-x86_64
	$(QEMU) $(QEMU_STDIO_FLAGS)

# ==========================================
# Limpieza
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
# Verificación de Herramientas
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
# Ayuda
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
