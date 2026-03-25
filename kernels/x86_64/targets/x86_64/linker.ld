/* ==========================================
 * Linker Script for GLOBEX_OS x86_64
 * ==========================================
 * Kernel loaded at 1MB (0x100000)
 * This address avoids conflicts with BIOS data areas
 */

ENTRY(start)

PHDRS
{
    /*
     * Segments with explicit permissions to eliminate RWX warning
     * Flags: 1 = X (execute), 2 = W (write), 4 = R (read)
     */
    text   PT_LOAD FLAGS(5);  /* R + E = 5  — executable code only */
    rodata PT_LOAD FLAGS(4);  /* R     = 4  — read-only data, no execute */
    data   PT_LOAD FLAGS(6);  /* R + W = 6  — read-write data */
}

SECTIONS
{
    /* Base address: 1MB */
    . = 1M;

    /* ==========================================
     * Boot Section - Multiboot Header
     * ========================================== */
    .boot : ALIGN(4K)
    {
        KEEP(*(.multiboot_header))
    } :text

    /* ==========================================
     * Code Section - Executable instructions
     * ========================================== */
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    } :text

    /* ==========================================
     * Read-Only Data - Constants, strings
     * ========================================== */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    /* ==========================================
     * Initialized Data - Global/static with initializer
     * ========================================== */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    } :data

    /* ==========================================
     * Uninitialized Data - BSS (zero-initialized)
     * ==========================================
     * This section will be zero-initialized by boot code.
     * Only C/C++ variables without explicit initializer go here.
     * ========================================== */
    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    /* ==========================================
     * Boot Data - Page tables and stack (NO initialize)
     * ==========================================
     * This section contains page tables and boot code stack.
     * It is NOBITS (does not occupy file space) but must NOT be zero-initialized
     * because it contains data configured at boot time.
     *
     * Separated from .bss to allow safe zero-initialization of BSS.
     * ========================================== */
    .boot.data (NOLOAD) : ALIGN(4K)
    {
        __boot_data_start = .;
        *(.boot.data)
        *(.bss.boot)  /* Compatibility with existing code */
        __boot_data_end = .;
    } :data

    __kernel_end = .;   /* End of kernel image — must be inside SECTIONS */

    /* ==========================================
     * Validations - ASSERTs for safety
     * ========================================== */
    ASSERT(. < 10M, "ERROR: Kernel size exceeds 10MB limit")
    ASSERT(ADDR(.text) % 4096 == 0, "ERROR: .text section not 4KB aligned")
    ASSERT(ADDR(.rodata) % 4096 == 0, "ERROR: .rodata section not 4KB aligned")
    ASSERT(ADDR(.data) % 4096 == 0, "ERROR: .data section not 4KB aligned")
    ASSERT(ADDR(.bss) % 4096 == 0, "ERROR: .bss section not 4KB aligned")
    ASSERT(ADDR(.boot.data) % 4096 == 0, "ERROR: .boot.data section not 4KB aligned")

    /* Verify page tables + guard page + stack fit in .boot.data
     * Layout: 4×4KB page tables + 1×4KB guard + 16×4KB stack = 21×4KB = 0x15000 */
    ASSERT((__boot_data_end - __boot_data_start) >= 0x15000,
           "ERROR: .boot.data too small for page tables + guard + stack")

    /* Discard debug sections */
    /DISCARD/ : { *(.stab) *(.stabstr) }
}

/* ==========================================
 * Useful global symbols for the kernel
 * ========================================== */
__kernel_start = 1M;

/* ==========================================
 * Memory Map Information
 * ========================================== */
__memory_mapped_start = 0x00000000;
__memory_mapped_end = 0x80000000;     /* 2GiB mapped (0-0x7FFFFFFF) */
__memory_mapped_size = 0x80000000;    /* 2GiB in bytes */

/* ==========================================
 * BSS symbols for boot initialization
 * ========================================== */
/* These symbols are used by boot code to zero-initialize BSS */
