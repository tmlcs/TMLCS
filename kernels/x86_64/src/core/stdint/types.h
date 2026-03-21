#ifndef CORE_TYPES_H
#define CORE_TYPES_H

/* =============================================================================
 * GLOBEX_OS Kernel-Specific Types
 * =============================================================================
 * 
 * Type-safe wrappers and architecture-specific types for GLOBEX_OS.
 * These types provide:
 *   - Type safety (prevent parameter swapping)
 *   - Clear semantics (vaddr_t vs paddr_t)
 *   - Documentation (purpose of each type)
 * 
 * @note Depends on stdint.h for base types
 * @note x86_64 architecture specific
 * =============================================================================
 */

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Address Types
 * =============================================================================
 * Distinguish between virtual and physical addresses for type safety.
 * Prevents accidentally using physical address where virtual is expected.
 * =============================================================================
 */

/**
 * @brief Virtual address (64-bit)
 * @note Used for all kernel virtual memory accesses
 * @note Range: 0x0000000000000000 - 0x00007FFFFFFFFFFF (canonical lower half)
 */
typedef uint64_t vaddr_t;

/**
 * @brief Physical address (64-bit)
 * @note Used for DMA, page tables, hardware registers
 * @note Range: 0x0000000000000000 - 0x0000FFFFFFFFFFFF (52-bit physical)
 */
typedef uint64_t paddr_t;

/**
 * @brief I/O port number (16-bit)
 * @note Used for port-mapped I/O (inb, outb instructions)
 * @note Range: 0x0000 - 0xFFFF (65536 ports)
 */
typedef uint16_t ioport_t;

/**
 * @brief I/O port value (8-bit)
 * @note Data value for port I/O operations
 */
typedef uint8_t ioval_t;

/* =============================================================================
 * Memory Management Types
 * =============================================================================
 */

/**
 * @brief Page frame number
 * @note Physical page index (page-aligned address / PAGE_SIZE)
 * @note Used for page table entries
 */
typedef uint64_t pfn_t;

/**
 * @brief Page directory index
 * @note Index into page directory (bits 39-47 of virtual address)
 */
typedef uint32_t pdx_t;

/**
 * @brief Page table index
 * @note Index into page table (bits 30-38 of virtual address)
 */
typedef uint32_t ptx_t;

/**
 * @brief Page offset within a frame
 * @note Offset within a 4KB page (bits 0-11 of address)
 */
typedef uint32_t poff_t;

/* =============================================================================
 * Segment Selector Types (GDT/LDT)
 * =============================================================================
 */

/**
 * @brief GDT/LDT selector value
 * @note Index into descriptor table + RPL + TI flag
 */
typedef uint16_t selector_t;

/**
 * @brief Segment descriptor access byte
 * @note Contains type, S, DPL, P flags
 */
typedef uint8_t seg_access_t;

/**
 * @brief Segment descriptor granularity
 * @note Contains limit 16-19 + AVL, L, D/B, G flags
 */
typedef uint8_t seg_gran_t;

/* =============================================================================
 * Interrupt Types (IDT)
 * =============================================================================
 */

/**
 * @brief Interrupt vector number
 * @note Range: 0-255 (IDT entry index)
 */
typedef uint8_t vector_t;

/**
 * @brief Interrupt type attribute
 * @note Contains P, DPL, type flags
 */
typedef uint8_t gate_attr_t;

/* =============================================================================
 * VGA Text Mode Types
 * =============================================================================
 */

/**
 * @brief VGA color attribute (4-bit)
 * @note Range: 0-15 (16 colors)
 * @note Used for foreground/background colors
 */
typedef uint8_t vga_color_t;

/**
 * @brief VGA row number
 * @note Range: 0-24 (25 rows)
 */
typedef uint8_t vga_row_t;

/**
 * @brief VGA column number
 * @note Range: 0-79 (80 columns)
 */
typedef uint8_t vga_col_t;

/**
 * @brief VGA screen cell (character + color)
 * @note 16-bit value: low byte = char, high byte = color
 */
typedef uint16_t vga_cell_t;

/* =============================================================================
 * Serial Port Types
 * =============================================================================
 */

/**
 * @brief Serial baud rate
 * @note Common values: 9600, 19200, 115200
 */
typedef uint32_t baud_t;

/**
 * @brief Serial register offset
 * @note Offset from base I/O port
 */
typedef uint8_t serial_reg_t;

/* =============================================================================
 * Error Code Types
 * =============================================================================
 */

/**
 * @brief Kernel error code
 * @note 0 = success, non-zero = error
 */
typedef int error_t;

/**
 * @brief Boolean result
 * @note 0 = false, 1 = true
 */
typedef int bool_t;

/* =============================================================================
 * Inline Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert physical address to virtual address
 * @param phys Physical address
 * @return Virtual address (identity mapped in kernel)
 * @note Assumes identity mapping for physical memory
 */
static inline vaddr_t phys_to_virt(paddr_t phys) {
    return (vaddr_t)phys;  /* Identity mapped */
}

/**
 * @brief Convert virtual address to physical address
 * @param virt Virtual address
 * @return Physical address
 * @note Assumes identity mapping for kernel memory
 */
static inline paddr_t virt_to_phys(vaddr_t virt) {
    return (paddr_t)virt;  /* Identity mapped */
}

/**
 * @brief Align address up to page boundary
 * @param addr Address to align
 * @return Page-aligned address (rounded up)
 */
static inline vaddr_t align_up(vaddr_t addr) {
    return (addr + 0xFFF) & ~0xFFFULL;
}

/**
 * @brief Align address down to page boundary
 * @param addr Address to align
 * @return Page-aligned address (rounded down)
 */
static inline vaddr_t align_down(vaddr_t addr) {
    return addr & ~0xFFFULL;
}

/**
 * @brief Check if address is page-aligned
 * @param addr Address to check
 * @return true if aligned, false otherwise
 */
static inline bool_t is_aligned(vaddr_t addr) {
    return (addr & 0xFFF) == 0 ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* CORE_TYPES_H */
