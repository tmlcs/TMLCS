# Variables
CC := x86_64-elf-g++
CCFLAGS := -Wall -Wextra -Werror -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore -ffreestanding -I src/intf
AS := nasm
ASFLAGS := -f elf64
LD := x86_64-elf-ld
LDSCRIPT := targets/x86_64/linker.ld
GRUB := grub-mkrescue
ISO_DIR := dist/x86_64
BUILD_DIR := build
KERNEL_BIN := $(ISO_DIR)/kernel.bin
ISO_FILE := $(ISO_DIR)/kernel.iso
GLOBEX_BIN := $(ISO_DIR)/globex.bin
GLOBEX_ISO_FILE := $(ISO_DIR)/globex.iso

# Archivos de código fuente y objetos
kernel_source_files := $(shell find src/impl/kernel -name *.cpp)
kernel_object_files := $(patsubst src/impl/kernel/%.cpp, $(BUILD_DIR)/kernel/%.o, $(kernel_source_files))
kernel_dependency_files := $(patsubst src/impl/kernel/%.cpp, $(BUILD_DIR)/kernel/%.d, $(kernel_source_files))

x86_64_cpp_source_files := $(shell find src/impl/x86_64 -name *.cpp)
x86_64_cpp_object_files := $(patsubst src/impl/x86_64/%.cpp, $(BUILD_DIR)/x86_64/%.o, $(x86_64_cpp_source_files))
x86_64_cpp_dependency_files := $(patsubst src/impl/x86_64/%.cpp, $(BUILD_DIR)/x86_64/%.d, $(x86_64_cpp_source_files))

x86_64_asm_source_files := $(shell find src/impl/x86_64 -name *.asm)
x86_64_asm_object_files := $(patsubst src/impl/x86_64/%.asm, $(BUILD_DIR)/x86_64/%.o, $(x86_64_asm_source_files))

x86_64_object_files := $(x86_64_cpp_object_files) $(x86_64_asm_object_files)

# Reglas de compilación
$(kernel_object_files): $(BUILD_DIR)/kernel/%.o : src/impl/kernel/%.cpp
	mkdir -p $(dir $@) && \
	$(CC) $(CCFLAGS) $(DEPFLAGS) -c $< -o $@

$(x86_64_cpp_object_files): $(BUILD_DIR)/x86_64/%.o : src/impl/x86_64/%.cpp
	mkdir -p $(dir $@) && \
	$(CC) $(CCFLAGS) $(DEPFLAGS) -c $< -o $@

$(x86_64_asm_object_files): $(BUILD_DIR)/x86_64/%.o : src/impl/x86_64/%.asm
	mkdir -p $(dir $@) && \
	$(AS) $(ASFLAGS) $< -o $@

x86_64_build_files := $(kernel_object_files) $(x86_64_object_files)

kernel_files: $(kernel_object_files)
x86_64_asm_files: $(x86_64_asm_object_files)
x86_64_cpp_files: $(x86_64_cpp_object_files)

all_files: $(kernel_files) $(x86_64_asm_files) $(x86_64_cpp_files)

define define_iso_build
.PHONY: build-$(1)
build-$(1): $(3)
	mkdir -p $(ISO_DIR) && \
	$(LD) -n -o $(2) -T $(LDSCRIPT) $(3) && \
	cp $(2) targets/x86_64/iso/boot/$(1).bin && \
	$(GRUB) /usr/lib/grub/i386-pc -o $(ISO_DIR)/$(1).iso targets/x86_64/iso
endef

$(eval $(call define_iso_build,globex,$(GLOBEX_BIN),$(x86_64_build_files)))
$(eval $(call define_iso_build,kernel,$(KERNEL_BIN),$(kernel_object_files) $(x86_64_object_files)))

build-x86_64-verbose: $(kernel_object_files) $(x86_64_object_files)
	mkdir -p $(ISO_DIR) && \
	$(LD) -n -o $(KERNEL_BIN) -T $(LDSCRIPT) $(kernel_object_files) $(x86_64_object_files) && \
	cp $(KERNEL_BIN) targets/x86_64/iso/boot/kernel.bin && \
	$(GRUB) --verbose /usr/lib/grub/i386-pc -o $(ISO_FILE) targets/x86_64/iso

-include $(kernel_dependency_files) $(x86_64_cpp_dependency_files)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) dist

run:
	qemu-system-x86_64 -cdrom $(ISO_FILE) -serial file:serial.log