#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include <stdint.h>

// Multiboot 2 magic number
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

// Multiboot 2 information structure
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_string {
    uint32_t type;
    uint32_t size;
    char string[0];
};

struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t reserved;
    char string[0];
};

struct multiboot_tag_basic_meminfo {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    multiboot_mmap_entry entries[0];
};

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
};

struct multiboot_tag_elf_sections {
    uint32_t type;
    uint32_t size;
    uint32_t num;
    uint32_t entsize;
    uint32_t shndx;
    char sections[0];
};

struct multiboot_tag_bootdev {
    uint32_t type;
    uint32_t size;
    uint32_t biosdev;
    uint32_t slice;
    uint32_t part;
};

struct multiboot_tag_load_base_addr {
    uint32_t type;
    uint32_t size;
    uint32_t load_base_addr;
};

struct multiboot_info {
    uint32_t total_size;
    uint32_t reserved;
    multiboot_tag tags[0];
};

// Multiboot tag types
#define MULTIBOOT_TAG_TYPE_END                  0
#define MULTIBOOT_TAG_TYPE_CMDLINE              1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME     2
#define MULTIBOOT_TAG_TYPE_MODULE               3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO        4
#define MULTIBOOT_TAG_TYPE_BOOTDEV              5
#define MULTIBOOT_TAG_TYPE_MMAP                 6
#define MULTIBOOT_TAG_TYPE_VBE                  7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER          8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS         9
#define MULTIBOOT_TAG_TYPE_APM                  10
#define MULTIBOOT_TAG_TYPE_EFI32                11
#define MULTIBOOT_TAG_TYPE_EFI64                12
#define MULTIBOOT_TAG_TYPE_SMBIOS               13
#define MULTIBOOT_TAG_TYPE_ACPI_OLD             14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW             15
#define MULTIBOOT_TAG_TYPE_NETWORK              16
#define MULTIBOOT_TAG_TYPE_EFI_MMAP             17
#define MULTIBOOT_TAG_TYPE_EFI_BS               18
#define MULTIBOOT_TAG_TYPE_EFI32_IH             19
#define MULTIBOOT_TAG_TYPE_EFI64_IH             20
#define MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR       21

// ELF section flags
#define MB_ELF_SHF_WRITE        0x1
#define MB_ELF_SHF_ALLOC        0x2
#define MB_ELF_SHF_EXECINSTR    0x4
#define MB_ELF_SHF_MASKPROC     0xF0000000

struct multiboot_elf_section_header {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
};

#endif // MULTIBOOT2_H
