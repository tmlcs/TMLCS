# Variables
CC := x86_64-elf-g++
AS := nasm
LD := x86_64-elf-ld
GRUB := grub-mkrescue
ISO_DIR := dist/x86_64
BUILD_DIR := build
KERNEL_BIN := $(ISO_DIR)/kernel.bin
ISO_FILE := $(ISO_DIR)/kernel.iso

# Archivos de código fuente y objetos
kernel_source_files := $(shell find src/impl/kernel -name *.cpp)
kernel_object_files := $(patsubst src/impl/kernel/%.cpp, $(BUILD_DIR)/kernel/%.o, $(kernel_source_files))

x86_64_cpp_source_files := $(shell find src/impl/x86_64 -name *.cpp)
x86_64_cpp_object_files := $(patsubst src/impl/x86_64/%.cpp, $(BUILD_DIR)/x86_64/%.o, $(x86_64_cpp_source_files))

x86_64_asm_source_files := $(shell find src/impl/x86_64 -name *.asm)
x86_64_asm_object_files := $(patsubst src/impl/x86_64/%.asm, $(BUILD_DIR)/x86_64/%.o, $(x86_64_asm_source_files))

x86_64_object_files := $(x86_64_cpp_object_files) $(x86_64_asm_object_files)

# Reglas de compilación
$(kernel_object_files): $(BUILD_DIR)/kernel/%.o : src/impl/kernel/%.cpp
	mkdir -p $(dir $@) && \
	$(CC) -c -I src/intf -ffreestanding $< -o $@

$(x86_64_cpp_object_files): $(BUILD_DIR)/x86_64/%.o : src/impl/x86_64/%.cpp
	mkdir -p $(dir $@) && \
	$(CC) -c -I src/intf -ffreestanding $< -o $@

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
